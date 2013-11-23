CPP=g++
BCC=/opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-linux-gcc
BPP=/opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-linux-g++
CPPFLAGS=-g -I.. -Wall --pedantic -Wno-variadic-macros
BCFLAGS=-g -I.. -std=gnu99
BPPFLAGS=-g -I.. -Wall --pedantic -Wno-variadic-macros
LDFLAGS=-lrt -lvirt -lyajl -pthread 
BLDFLAGS=bridge/libbridge/*.o -pthread -lrt
SHELL=/bin/sh

all: lib fd_test bin/pfdserver bin/induce_failure bin/dummy bin/handlerd bin/dsm_srv bin/vanilla_cohort bin/falcon_cohort bin/vanilla_proxy bin/falcon_proxy bin/scheduling_load bin/memory_load bin/incrementer bin/diomedes_proxy bin/diomedes


.PHONY:
bin/pfdserver: lib apps/pfdserver/*.cc
	mkdir -p bin; cd apps; make
	cp apps/pfdserver/pfdserver bin

bin/induce_failure: lib apps/libfail/induce_failure.cc
	mkdir -p bin; cd apps; make
	g++ $(LDFLAGS) lib/libfalcon.a lib/config.o apps/libfail/induce_failure.o -o $@

bin/dummy: lib apps/libfail/dummy.cc
	mkdir -p bin; cd apps; make
	g++ $(LDFLAGS) lib/libfalcon.a lib/spy.o lib/config.o apps/libfail/libfail.o apps/libfail/dummy.o -o $@

bin/dsm_srv: lib apps/rsm/dsm_srv.cc
	mkdir -p bin; cd apps; make
	cp apps/rsm/dsm_{srv,client} bin

falcon/process_enforcer/spy.o:
	cd falcon; make

bin/incrementer: lib
	cp falcon/process_enforcer/incrementer bin/incrementer
	
bin/diomedes: lib apps/diomedes/diomedes.o
	g++ $(LDFLAGS) lib/libfalcon.a lib/spy.o lib/config.o apps/libfail/libfail.o apps/diomedes/diomedes.o -o $@

bin/diomedes_proxy: apps/diomedes/diomedes_proxy.o
	g++ $(LDFLAGS) lib/libfalcon.a lib/spy.o lib/config.o $^ -o $@

bin/handlerd: lib
	cp falcon/process_enforcer/process_enforcer bin/handlerd

apps/pmp_falcon_leader/cohort:
	cd apps; make

apps/pmp_vanilla/cohort:
	cd apps; make

apps/pmp_vanilla/clientproxy:
	cd apps; make

apps/pmp_falcon_leader/clientproxy:
	cd apps; make

bin/vanilla_cohort: apps/pmp_vanilla/cohort
	cp $^ $@

bin/falcon_cohort: apps/pmp_falcon_leader/cohort
	cp $^ $@

bin/vanilla_proxy: apps/pmp_vanilla/clientproxy
	cp $^ $@

bin/falcon_proxy: apps/pmp_falcon_leader/clientproxy
	cp $^ $@

bin/scheduling_load: lib apps/load/scheduling.o
	g++ $(LDFLAGS) lib/config.o apps/load/scheduling.o -o $@

bin/memory_load: lib apps/load/memory.o
	g++ $(LDFLAGS) lib/config.o apps/load/memory.o -o $@

apps/load/schedule.o:
	cd apps; make

apps/load/memory.o:
	cd apps; make

lib: falcon/process_enforcer/spy.o falcon/client/libfalcon.a
	mkdir -p lib
	cp falcon/process_enforcer/spy.o lib
	cp falcon/process_enforcer/config.o lib
	cp falcon/client/libfalcon.a lib

# This makes only on the vms.
kernel_module: kernel_module/*.c
	cd kernel_module; make

# This makes the fd_test files.
fd_test: lib
	cd fd_test; make

clean:
	rm -rf lib bin demo test
	cd kernel_module; make clean
	cd fd_test; make clean
	cd falcon; make clean
	cd apps; make clean
