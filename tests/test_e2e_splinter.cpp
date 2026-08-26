#include "test_e2e_common.hpp"
#include "splinter_reverse_map.hpp"

int main(int argc, char **argv) {
  int qbits = 12;
  int num_queries = 100000;
  double fill_factor = 0.25;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--qbits") == 0 || strcmp(argv[i], "-q") == 0) {
      if (i + 1 < argc) qbits = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--num-queries") == 0 || strcmp(argv[i], "-n") == 0) {
      if (i + 1 < argc) num_queries = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--load-factor") == 0 || strcmp(argv[i], "-l") == 0) {
      if (i + 1 < argc) fill_factor = atof(argv[++i]);
    } else {
      fprintf(stderr, "Usage: %s [--qbits N] [--num-queries N] [--load-factor F]\n", argv[0]);
      return 1;
    }
  }

  return run_aqf<SplinterDB>("splinter", false, qbits, num_queries, fill_factor);
}