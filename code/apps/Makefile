all: pfdserver/pfdserver libfail/libfail.o libfail/induce_failure.o rsm/rsm_srv pmp_vanilla/cohort pmp_falcon_leader/cohort load/schedule.o diomedes/diomedes.o

.PHONY:

diomedes/diomedes.o:
	cd diomedes; make

load/schedule.o:
	cd load; make

pfdserver/pfdserver:
	cd pfdserver; make

libfail/libfail.o:
	cd libfail; make

libfail/induce_failure.o:
	cd libfail; make

rsm/rsm_srv:
	cd rsm; make

pmp_vanilla/cohort:
	cd pmp_vanilla; make

pmp_falcon_leader/cohort:
	cd pmp_falcon_leader; make

clean:
	cd pfdserver; make clean
	cd libfail; make clean
	cd rsm; make clean
	cd pmp_vanilla; make clean
	cd pmp_falcon_leader; make clean
	cd load; make clean
	cd diomedes; make clean
