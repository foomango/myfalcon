# makefile based on dm's
CFLAGS	:= -g -Wall --pedantic
CXXFLAGS:= -g -Wall --pedantic -I../../include -Wno-variadic-macros
LDFLAGS := -lpthread -lrt -lyajl -lvirt ../../lib/*.o
RPCGENS	:= dsm_svc.c dsm_clnt.c dsm_xdr.c dsm.h
PROGS	:= dsm_client dsm_srv

all:	$(PROGS)

dsm_client:	dsm_clnt.o dsm_xdr.o dsm_client.o
	$(CXX) -o $@ $^ $(LDFLAGS)

dsm_srv:	dsm_svc.o dsm_xdr.o dsm_srv.o dsm_ops.o
	$(CXX) -o $@ $^ $(LDFLAGS)

$(RPCGENS): dsm.x
	rpcgen -M dsm.x
	rpcgen -M -m dsm.x > dsm_svc.c

*.o:	dsm.h

clean:
	rm -f *.o $(RPCGENS) $(PROGS)

