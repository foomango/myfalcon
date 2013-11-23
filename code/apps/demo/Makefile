CXXFLAGS	:= -I../../falcon -I../../include -Wall -g ../../falcon/common/config.cc
# TODO(leners) eventually the libs should live in ../lib and libfalcon should include process observer.
LDFLAGS		:= -lrt -pthread -lyajl ../../falcon/client/libfalcon.a ../../falcon/process_enforcer/spy.o

%.cc:
	g++ $(CXXFLAGS) $(LDFLAGS) -o $* $*.cc
