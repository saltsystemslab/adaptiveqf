#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <vector>

#include "aqf_wrapper.hpp"

extern "C" {
int qf_insert_using_ll_table(QF *qf, uint64_t key, uint64_t count,
                             qf_insert_result *result, uint8_t flags);
}

template <typename ReverseMap>
static int run_aqf(const char *label, bool legacy, int qbits, int num_queries, double fill_factor) {
  const double max_load_factor = 0.95;

  int num_inserts = (int)((1ULL << qbits) * fill_factor);
  if (num_inserts < 1) num_inserts = 1;

  QFilterConfig cfg;
  cfg.qbits = qbits;
  cfg.rbits = 8;
  cfg.max_load_factor = max_load_factor;
  cfg.hash_mode = QF_HASH_NONE;
  cfg.reverse_map.storage_path = "test_reverse_map";
  cfg.reverse_map.cache_size_bytes = 64ULL * 1024 * 1024;
  cfg.reverse_map.overwrite = true;

  AdaptiveQF<ReverseMap> filter;
  if (filter.init(cfg)) {
    fprintf(stderr, "[%s] Failed to init filter\n", label);
    return 1;
  }

  srand(42);
  std::vector<uint64_t> inserted(num_inserts);
  for (int i = 0; i < num_inserts; i++) {
    inserted[i] = ((uint64_t)rand() << 32) | rand();
  }

  if (legacy) {
    for (int i = 0; i < num_inserts; i++) {
      qf_insert_result result;
      int ret = qf_insert_using_ll_table(filter.getQF(), inserted[i], 1, &result,
                                          QF_NO_LOCK | QF_KEY_IS_HASH);
      if (ret < 0) {
        fprintf(stderr, "[%s] Failed bulk insert at %d: %d\n", label, i, ret);
        return 1;
      }
      filter.getReverseMap()->insert(result.minirun_id,
                                             result.minirun_rank, inserted[i]);
    }
  } else {
    if (int ret = filter.bulkInsert(inserted.data(), num_inserts)) {
      fprintf(stderr, "[%s] Failed bulk insert: %d\n", label, ret);
      return 1;
    }
  }

  // Pre-check: all inserted keys should be present
  for (int i = 0; i < num_inserts; i++) {
    QFilterQueryResult result;
    filter.queryFilter(inserted[i], &result);
    if (!result.key_present) {
      fprintf(stderr, "[%s] FALSE NEGATIVE (pre): inserted key %lx not found\n", label, inserted[i]);
      return 1;
    }
  }
  printf("[%s] Pre-check: all %d inserted keys found\n", label, num_inserts);

  std::unordered_set<uint64_t> adapted_keys;
  int total_fps = 0;
  int adapted_count = 0;
  int i;

  for (i = 0; i < num_queries; i++) {
    uint64_t q = ((uint64_t)rand() << 32) | rand();
    QFilterQueryResult result;
    filter.queryFilter(q, &result);

    if (result.key_present) {
      // Skip true positives — confirm by scanning the data set (O(n)).
      if (std::find(inserted.begin(), inserted.end(), q) != inserted.end()) {
        continue;
      }
      total_fps++;

      int ret = filter.adapt(q, &result);
      if (ret < 0) {
        fprintf(stderr, "[%s] FAIL: adapt returned %d for key %lx\n", label,
                ret, q);
        return 1;
      }
      adapted_keys.insert(q);
      adapted_count++;

      filter.queryFilter(q, &result);
      if (result.key_present) {
        fprintf(stderr, "[%s] FAIL: adapted key %lx still reports as present "
                "(hash=%lx rank=%d count=%d)\n",
                label, q, result.hash, result.minirun_rank, result.count);
        return 1;
      }
    }

    if (filter.loadFactor() >= max_load_factor) {
      printf("[%s] Stopping early at i=%d: load factor reached %.2f\n", label, i, max_load_factor);
      break;
    }
  }

  // Post-check: all inserted keys should still be present
  int fn_count = 0;
  for (int i = 0; i < num_inserts; i++) {
    QFilterQueryResult result;
    filter.queryFilter(inserted[i], &result);
    if (!result.key_present) {
      fn_count++;
      if (fn_count <= 5)
        fprintf(stderr, "[%s] FALSE NEGATIVE (post): inserted key %lx not found\n", label, inserted[i]);
    }
  }
  printf("[%s] Post-check: %d / %d inserted keys missing\n", label, fn_count, num_inserts);

  printf("[%s] Inserted: %d\n", label, num_inserts);
  printf("[%s] Queries: %d\n", label, i);
  printf("[%s] Total false positives: %d\n", label, total_fps);
  printf("[%s] Adapted: %d\n", label, adapted_count);
  printf("[%s] Unique adapted keys: %zu\n", label, adapted_keys.size());
  printf("[%s] Final load factor: %f\n", label, filter.loadFactor());
  printf("[%s] False positive rate: %f\n", label, (double)total_fps / i);

  filter.close();
  remove(cfg.reverse_map.storage_path.c_str());
  return fn_count > 0 ? 1 : 0;
}
