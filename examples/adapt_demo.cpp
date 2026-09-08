#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include "aqf_wrapper.hpp"
#include "localmap_db.hpp"

int main() {
  AdaptiveQF<LocalMap> filter;
  if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_DEFAULT)) return 1;

  srand(42);
  uint64_t keys[700];
  for (int i = 0; i < 700; i++) keys[i] = ((uint64_t)rand() << 32) | rand();
  if (filter.bulkInsert(keys, 700) < 0) {
    fprintf(stderr, "Failed to insert keys\n");
    filter.close();
    return 1;
  }
  printf("Inserted 700 keys\n");

  for (int i = 0; i < 100000; i++) {
    uint64_t query = ((uint64_t)rand() << 32) | rand();
    QFilterQueryResult result;
    if (filter.queryFilter(query, &result) < 0) {
      fprintf(stderr, "Failed to query filter\n");
      filter.close();
      return 1;
    }

    if (!result.key_present) {
      continue; // true negative — filter correctly says "not present"
    }

    // Scan the data set to confirm this is a false positive (O(n)).
    if (std::find(keys, keys + 700, query) != keys + 700) {
      continue; // true positive — filter correctly says "present"
    }

    // False positive — adapt to correct it.
    printf("False positive at query %d: key=%lx  rank=%d\n",
           i, query, result.minirun_rank);

    if (filter.adapt(query, &result) < 0) {
      fprintf(stderr, "Failed to adapt false positive\n");
      filter.close();
      return 1;
    }

    printf("  Adapted, key no longer a false positive\n");
    if (filter.queryFilter(query, &result) < 0) {
      fprintf(stderr, "Failed to re-query filter\n");
      filter.close();
      return 1;
    }
    printf("  Re-query: present=%d\n", result.key_present);
    break;
  }

  filter.close();
  return 0;
}
