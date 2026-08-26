#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "aqf_wrapper.hpp"
#include "splinter_reverse_map.hpp"

int main() {
  ReverseMapConfig rm_cfg;
  rm_cfg.storage_path = "./splinter_demo_db";
  rm_cfg.cache_size_bytes = 64ULL * 1024 * 1024;
  rm_cfg.overwrite = true;

  AdaptiveQF<SplinterDB> filter;
  if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_NONE, rm_cfg)) {
    fprintf(stderr, "Failed to init filter with SplinterDB\n");
    return 1;
  }

  srand(42);
  uint64_t keys[700];
  for (int i = 0; i < 700; i++) keys[i] = ((uint64_t)rand() << 32) | rand();
  filter.bulkInsert(keys, 700);
  printf("Inserted 700 keys\n");

  for (int i = 0; i < 100000; i++) {
    uint64_t q = ((uint64_t)rand() << 32) | rand();
    QFilterQueryResult r;
    filter.queryFilter(q, &r);
    if (!r.key_present) continue;

    printf("False positive at query %d: key=%lx  rank=%d\n",
           i, q, r.minirun_rank);

    int n = filter.adapt(q, &r);

    printf("  Adapted, key no longer a false positive\n");
    filter.queryFilter(q, &r);
    printf("  Re-query: present=%d\n", r.key_present);
    break;
  }

  filter.close();
  remove(rm_cfg.storage_path.c_str());
  return 0;
}