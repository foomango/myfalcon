NTFA_OPTS	:= -DPRIMARY_PING=100 -DPRIMARY_PING_PERIOD=1000
CXXFLAGS	:= -I/usr/local/include/sfslite -I.. -g $(NTFA_OPTS)
LDFLAGS		:= -L/usr/local/lib/sfslite -lsfscrypt -larpc -lasync -lresolv -lgmp -lsfsmisc ../../lib/*.o -pthread -lvirt -lrt -pthread -lyajl
HEADERS		:= rep.h util.h execbackend.h exitcb.h set.h bcast.h execute.h
OBJS		:= clientproxy.o cohort.o rep.o bcast.o execbackend.o exitcb.o groupstat.o

all: clientproxy cohort groupstat groupleave pfdserver

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

pfdserver: pfdserver.cc
	$(CXX) $(CXXFLAGS) pfdserver.cc ../../lib/*.o -pthread -lvirt -lrt -pthread -lyajl -o pfdserver

clean:
	rm -f pfdserver cohort clientproxy groupstat groupleave rep.h rep.C *.o

$(OBJS): $(HEADERS)

.PRECIOUS: rep.C
%.C: %.x
	PATH=$$PATH:/usr/local/lib/sfslite rpcc -c -o $@ $<

%.h: %.x
	PATH=$$PATH:/usr/local/lib/sfslite rpcc -h -o $@ $<

