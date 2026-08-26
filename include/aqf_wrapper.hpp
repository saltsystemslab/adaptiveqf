#ifndef AQF_WRAPPER_HPP
#define AQF_WRAPPER_HPP

#include <cstddef>
#include <cstdint>

#include "aqf.h"
#include "reverse_map_config.hpp"

struct QFilterConfig {
  size_t qbits;
  size_t rbits;
  double max_load_factor;
  int hash_mode;
  ReverseMapConfig reverse_map;
};

struct QFilterQueryResult {
  int key_present;
  int minirun_rank;
  int count;
  uint64_t hash;
};

static inline uint64_t ceil_log2(uint64_t x) {
  if (x <= 1) return 0;
  return 64 - __builtin_clzll(x - 1);
}

template <typename ReverseMap>
class AdaptiveQF {
public:
  int init(QFilterConfig cfg) {
    config = cfg;
    size_t num_slots = 1ull << config.qbits;
    if (!aqf_malloc(&qf, num_slots, config.qbits + config.rbits, 0,
                    (enum qf_hashmode)config.hash_mode, 0)) {
      return -1;
    }
    reverseMap.init(config.reverse_map, config.qbits + config.rbits);
    fullPoint = config.max_load_factor * (1ULL << config.qbits);
    return 0;
  }

  // Compute q based on the expected number of elements, so the caller
  // does not need to estimate the number of slots directly.
  int initializeForCapacity(uint64_t numExpectedElements, size_t rbits,
                            double max_load_factor, int hash_mode,
                            ReverseMapConfig reverseMapConfig = {}) {
    uint64_t num_slots = (uint64_t)((double)numExpectedElements / max_load_factor) + 1;
    QFilterConfig cfg;
    cfg.qbits = (size_t)ceil_log2(num_slots);
    cfg.rbits = rbits;
    cfg.max_load_factor = max_load_factor;
    cfg.hash_mode = hash_mode;
    cfg.reverse_map = reverseMapConfig;
    return init(cfg);
  }

  // Initialize directly from the number of quotient bits, which sets the
  // number of slots to 1 << qbits.
  int initializeWithQBits(size_t qbits, size_t rbits,
                          double max_load_factor, int hash_mode,
                          ReverseMapConfig reverseMapConfig = {}) {
    QFilterConfig cfg;
    cfg.qbits = qbits;
    cfg.rbits = rbits;
    cfg.max_load_factor = max_load_factor;
    cfg.hash_mode = hash_mode;
    cfg.reverse_map = reverseMapConfig;
    return init(cfg);
  }

  int bulkInsert(uint64_t *keys, uint64_t numKeys) {
    uint8_t flags = QF_NO_LOCK;
    if (config.hash_mode == QF_HASH_NONE) flags |= QF_KEY_IS_HASH;

    uint64_t *fingerprints = new uint64_t[numKeys];
    uint64_t *ranks = new uint64_t[numKeys];
    for (uint64_t i = 0; i < numKeys; i++) {
      qf_insert_result result;
      int ret = aqf_insert(&qf, keys[i], 1, &result, flags);
      if (ret < 0) { delete[] fingerprints; delete[] ranks; return ret; }
      fingerprints[i] = result.minirun_id;
      ranks[i] = result.minirun_rank;
    }
    reverseMap.bulkInsert(keys, fingerprints, ranks, numKeys);
    delete[] fingerprints;
    delete[] ranks;
    return 0;
  }

  int queryFilter(uint64_t queryKey, QFilterQueryResult *result) {
    uint8_t flags = (config.hash_mode == QF_HASH_NONE) ? QF_KEY_IS_HASH : 0;
    uint8_t minirun_rank;
    uint64_t hash;
    uint64_t count = aqf_query(&qf, queryKey, &hash, &minirun_rank,
                               flags);
    if (count > 0) {
      result->key_present = 1;
      result->count = count;
    } else {
      result->key_present = 0;
    }
    result->hash = hash;
    result->minirun_rank = minirun_rank;
    return 0;
  }

  // Correct a false positive for queryKey, extending the fingerprint of the
  // original key that caused the collision. One round of extension removes
  // the false positive for one minirun rank. filterResult must describe a
  // current false positive for queryKey (e.g. from queryFilter). Returns the
  // number of rounds performed, or 0 if queryKey is not (longer) a false
  // positive.
  int adapt(uint64_t queryKey, QFilterQueryResult *filterResult) {
    int count = 0;
    for (;;) {
      if (!filterResult->key_present) break;
      int ret = adaptOnce(queryKey, filterResult);
      if (ret < 0) return ret;
      count++;
      queryFilter(queryKey, filterResult);
    }
    return count;
  }

  // Perform a single fingerprint extension for queryKey. Requires
  // result to describe a current false positive for queryKey.
  int adaptOnce(uint64_t queryKey, QFilterQueryResult *filterResult) {
    if (aqf_get_noccupied_slots(&qf) >= fullPoint) {
      return -1;
    }
    uint8_t flags = (config.hash_mode == QF_HASH_NONE) ? QF_KEY_IS_HASH : 0;
    uint64_t origKey;
    uint64_t fingerprint = filterResult->hash & BITMASK(config.qbits + config.rbits);
    int ret = reverseMap.getFingerprint(fingerprint, filterResult->minirun_rank,
                                         &origKey);
    if (ret) return -1;
    return aqf_adapt(&qf, origKey, queryKey, filterResult->minirun_rank,
                     flags);
  }

  double loadFactor() {
    return (double)aqf_get_noccupied_slots(&qf) / aqf_get_nslots(&qf);
  }

  int close() {
    aqf_free(&qf);
    reverseMap.close();
    return 0;
  }

  uint64_t sizeInBytes() {
    return aqf_get_total_size_in_bytes(&qf);
  }

  void dumpMetadata() { aqf_dump_metadata(&qf); }
  void dump() { aqf_dump(&qf); }

  void dumpBlock(uint64_t blockIndex) {
    uint64_t start_q = blockIndex * QF_SLOTS_PER_BLOCK;
    qf_slot_debug buf[128];
    uint64_t n;
    int ret = aqf_read_quotient_slots(&qf, start_q, QF_SLOTS_PER_BLOCK,
                                       buf, 128, &n);
    if (ret < 0) { return; }

    uint64_t bits_per_slot = qf.metadata->bits_per_slot;
    uint64_t qbits = qf.metadata->quotient_bits;

    fprintf(stdout, "Block %lu (offset=%u):\n", blockIndex,
            get_block(&qf, blockIndex)->offset);

    for (uint64_t i = 0; i < n; i++) {
      if (buf[i].is_extension) continue;

      uint64_t quotient = buf[i].quotient;
      uint64_t remainder = buf[i].slot;

      int rank = 0;
      for (uint64_t k = 0; k < i; k++) {
        if (!buf[k].is_extension && buf[k].quotient == quotient &&
            buf[k].slot == remainder) {
          rank++;
        }
      }

      uint64_t minirun_id = (quotient << bits_per_slot) | remainder;
      fprintf(stdout, "  q=%lu rem=%lu rank=%d", quotient, remainder, rank);

      fprintf(stdout, " exts:");
      for (uint64_t k = i + 1; k < n && buf[k].is_extension; k++) {
        fprintf(stdout, " %lu", buf[k].slot);
      }

      uint64_t key;
      if (reverseMap.getFingerprint(minirun_id, rank, &key) == 0) {
        fprintf(stdout, " key=%lx", key);
      }
      fprintf(stdout, "\n");
    }
  }

  QF *getQF() { return &qf; }
  ReverseMap *getReverseMap() { return &reverseMap; }

protected:
  QF qf;
  ReverseMap reverseMap;
  QFilterConfig config;
  size_t fullPoint;
};

#endif