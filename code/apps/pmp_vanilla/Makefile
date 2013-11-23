NTFA_OPTS	:= -DPRIMARY_PING=5 -DPRIMARY_PING_PERIOD=10
CXXFLAGS	:= -I/usr/include/sfslite -I.. -g $(NTFA_OPTS)
LDFLAGS		:= -L/usr/lib/sfslite -Wl,-rpath -Wl,/usr/lib/sfslite -lsfscrypt -larpc -lasync -lresolv -lgmp -lsfsmisc ../libfail/libfail.o -pthread
HEADERS		:= rep.h util.h execbackend.h exitcb.h set.h bcast.h execute.h
OBJS		:= clientproxy.o cohort.o rep.o bcast.o execbackend.o exitcb.o groupstat.o

all: clientproxy cohort groupstat groupleave

.PHONY:	submit.tar.gz
submit.tar.gz:
	tar -zcf submit.tar.gz Makefile *.[Cchx] answers.txt

turnin:	submit.tar.gz
	uuencode submit.tar.gz lab2.tar.gz | mail -s "cs244b lab submission" cs244b-staff@scs.stanford.edu,${USER}@stanford.edu

clientproxy: clientproxy.o rep.o bcast.o
	$(CXX) $^ -o $@ $(LDFLAGS)

cohort: cohort.o rep.o execbackend.o bcast.o exitcb.o
	$(CXX) $^ -o $@ $(LDFLAGS)

groupstat: groupstat.o rep.o bcast.o
	$(CXX) $^ -o $@ $(LDFLAGS)

groupleave: groupleave.o rep.o bcast.o
	$(CXX) $^ -o $@ $(LDFLAGS)

clean:
	rm -f cohort clientproxy groupstat groupleave rep.h rep.C *.o

$(OBJS): $(HEADERS)

.PRECIOUS: rep.C
%.C: %.x
	PATH=$$PATH:/usr/lib/sfslite rpcc -c -o $@ $<

%.h: %.x
	PATH=$$PATH:/usr/lib/sfslite rpcc -h -o $@ $<

