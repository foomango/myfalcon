CXXFLAGS	:= -Wall -fPIC -g -I.. -I${PROJECT_INCLUDES}
LDFLAGS		:= -lpthread -lrt
HEADERS		:= client.h layer.h
GENERATED	:= client_prot_xdr.cc spy_prot_xdr.cc spy_prot_clnt.cc client_prot_svc.cc spy_prot.h client_prot.h status.h
OBJS		:= client_prot_xdr.o spy_prot_xdr.o spy_prot_clnt.o client_prot_svc.o client.o layer.o client_ops.o

all: libfalcon.a test_client

.PHONY:

libfalcon.a: $(OBJS)
	$(LD) -r -o $@ $(OBJS)

test_client: libfalcon.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -L. -lfalcon test.cc -o $@

$(OBJS): $(GENERATED) $(HEADERS)

$(GENERATED): rpc

rpc: client_prot.x ../enforcer/spy_prot.x
	rpcgen -M client_prot.x
	cp ../enforcer/spy_prot.x .
	test -e status.h || ln -s ../enforcer/status.h .
	rpcgen -M spy_prot.x
	rm spy_prot.x
	rm spy_prot_svc.c
	rm client_prot_clnt.c
	sed -i 's/static//' client_prot_svc.c
	sed -i 's/main/__unused_main_/' client_prot_svc.c
	sed -i '/buf/d' *_xdr.c
	rename .c .cc *.c

clean:
	rm -fr $(GENERATED) *.o test_client libfalcon.a
