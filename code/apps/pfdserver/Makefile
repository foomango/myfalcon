CXXFLAGS	:= -Wall -g -I../../falcon -I ../../old -I../../include 
OBJS		:= config.o pfdserver.o ../../lib/libfalcon.a
LDFLAGS		:= -lyajl -lpthread -lrt

all: pfdserver

.PHONY:

pfdserver: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $@

config.o: ../../falcon/common/config.cc
	$(CXX) $(CXXFLAGS) $^ -c


pfdserver.o: pfdserver.cc

clean:
	rm -fr *.o pfdserver
