include Makefile.defs

BRCM	:= $(shell test -d /opt/brcm && echo -n yes)
all: client/test_client process_enforcer/process_enforcer os_enforcer/os_enforcer vmm_enforcer/vmm_enforcer

.PHONY:

process_enforcer/process_enforcer: enforcer/fake_enforcer
	cd process_enforcer; make -j3

os_enforcer/os_enforcer: enforcer/fake_enforcer
	cd os_enforcer; make -j3

vmm_enforcer/vmm_enforcer: enforcer/fake_enforcer
ifdef BRCM
	cd vmm_enforcer; make -j3
else
	touch vmm_enforcer/vmm_enforcer
endif

client/test_client:
	cd client; make

enforcer/fake_enforcer:
	cd enforcer; make -j3

clean:
	cd enforcer; make clean
	cd process_enforcer; make clean
	cd os_enforcer; make clean
	cd vmm_enforcer; make clean
	cd client; make clean
