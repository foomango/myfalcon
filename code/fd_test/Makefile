JAVAC = javac
CLASSPATH = lib/
BINPATH = bin/
SRCPATH = src/
TESTPATH = test/
JAVAFLAG = -d $(BINPATH) -classpath $(CLASSPATH)
FDPATH = org/apache/zookeeper/common/fd/
FDTESTPATH = org/apache/zookeeper/test/fd/
JSONPATH = org/json/
FDSOURCE = $(wildcard $(SRCPATH)$(FDPATH)*.java)
FDTESTSOURCE = $(wildcard $(TESTPATH)$(FDPATH)*.java)
FDTARGET = $(patsubst $(SRCPATH)$(FDPATH)%.java, $(BINPATH)$(FDPATH)%.class, $(FDSOURCE))
FDTESTTARGET = $(patsubst $(TESTPATH)$(FDTESTPATH)%.java, $(BINPATH)$(FDTESTPATH)%.class, $(FDTESTSOURCE))

#all: $(FDTARGET) $(FDTESTTARGET)

#$(BINPATH)$(FDPATH)%.class: $(SRCPATH)$(FDPATH)%.java
#	$(JAVAC) $(JAVAFLAG) $<

#$(BINPATH)$(FDTESTPATH)%.class: $(TESTPATH)$(FDTESTPATH)%.java
#	$(JAVAC) $(JAVAFLAG) $<

all:
	mkdir -p $(BINPATH); $(JAVAC) $(JAVAFLAG) $(SRCPATH)$(JSONPATH)*.java $(SRCPATH)$(FDPATH)*.java $(TESTPATH)$(FDTESTPATH)*.java

clean: 
	rm -f $(BINPATH)$(FDPATH)*.class $(BINPATH)$(FDTESTPATH)*.class $(BINPATH)$(JSONPATH)*.class
