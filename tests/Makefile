# Build tests against the AQF library.

CXX = g++ -std=c++17
CXXFLAGS = -O3 -I.. -I../include \
           -I../reverse_maps/ll_table \
           -I../reverse_maps/localmap \
           -I../reverse_maps/splinterdb \
           -I../external/splinterdb/include \
           -DSPLINTERDB_PLATFORM_DIR=platform_linux \
           -DSKIP_BOOL_DEF -D_GNU_SOURCE

SPLINTERPATH = ../external/splinterdb/build/release/lib
BASE_LDFLAGS = -lpthread -lm
SPLINTER_LDFLAGS = -L$(SPLINTERPATH) -lsplinterdb -laio -lxxhash \
                   -Wl,-rpath='$$ORIGIN/../external/splinterdb/build/release/lib'

all: test_e2e test_e2e_splinter

test_e2e: test_e2e_localmap.cpp ../libaqf.a
	$(CXX) $(CXXFLAGS) -o $@ $^ $(BASE_LDFLAGS)

test_e2e_splinter: test_e2e_splinter.cpp ../libaqf.a ../libsplinterutil.a
	$(CXX) $(CXXFLAGS) -o $@ $^ $(BASE_LDFLAGS) $(SPLINTER_LDFLAGS)

.PHONY: clean
clean:
	rm -f test_e2e test_e2e_splinter