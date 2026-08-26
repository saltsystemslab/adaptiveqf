#include "test_e2e_common.hpp"
#include "localmap_db.hpp"

int main(int argc, char **argv) {
  int qbits = 12;
  int num_queries = 100000;
  double fill_factor = 0.25;
  bool run_legacy = false;
  bool run_aqf_local = true;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--legacy") == 0) {
      run_legacy = true;
      run_aqf_local = false;
    } else if (strcmp(argv[i], "--both") == 0) {
      run_legacy = true;
      run_aqf_local = true;
    } else if (strcmp(argv[i], "--qbits") == 0 || strcmp(argv[i], "-q") == 0) {
      if (i + 1 < argc) qbits = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--num-queries") == 0 || strcmp(argv[i], "-n") == 0) {
      if (i + 1 < argc) num_queries = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--load-factor") == 0 || strcmp(argv[i], "-l") == 0) {
      if (i + 1 < argc) fill_factor = atof(argv[++i]);
    } else {
      fprintf(stderr, "Usage: %s [--legacy|--both] [--qbits N] [--num-queries N] [--load-factor F]\n", argv[0]);
      return 1;
    }
  }

  if (run_legacy) {
    if (run_aqf<LocalMap>("legacy", true, qbits, num_queries, fill_factor)) return 1;
  }
  if (run_aqf_local) {
    if (run_aqf<LocalMap>("aqf_local", false, qbits, num_queries, fill_factor)) return 1;
  }
  return 0;
}