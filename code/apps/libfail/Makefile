CXXFLAGS	:= -g -I ../../falcon -I../../include

all: libfail.o induce_failure.o dummy.o

.PHONY:

libfail.o: saboteur.cc
	$(CXX) $(CXXFLAGS) $^ -c -o $@

induce_failure.o: induce_failure.cc

dummy.o: dummy.cc

clean:
	rm -fr *.o
