ALL: default 

CC           = gcc
CLINKER      = $(CC)
OPTFLAGS     = -O0


SHELL = /bin/sh

CFLAGS  =   -DREENTRANT -Wunused -Wall -g -std=gnu99
CCFLAGS = $(CFLAGS)
LIBS =  -lpthread

EXECS = common.o dsmexec dsmwrap truc

default: $(EXECS)

dsmexec: dsmexec.o common.o dsmexec_utils.o
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-if [ ! -d "bin" ]; then mkdir bin ; fi
	@-mv $@ ./bin/

dsmwrap: dsmwrap.o common.o
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-if [ ! -d "bin" ]; then mkdir bin ; fi
	@-mv $@ ./bin/

truc: truc.o common.o
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-if [ ! -d "bin" ]; then mkdir bin ; fi
	mv $@ ./bin/

clean:
	@-/bin/rm -f *.o *~ PI* $(EXECS) *.out core 

veryclean: clean
	@-/bin/rm -f ./bin/*
	@-/bin/rmdir ./bin
	rm -rf ~/DSM_bin

.c:
	$(CC) $(CFLAGS) -o $* $< $(LIBS)
.c.o:
	$(CC) $(CFLAGS) -c $<
.o:
	${CLINKER} $(OPTFLAGS) -o $* $*.o $(LIBS)
