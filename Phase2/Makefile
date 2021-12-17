ALL: default 

CC       = gcc
CLINKER  = $(CC)
OPTFLAGS = -O0 


SHELL = /bin/sh

CFLAGS  = -DREENTRANT -Wunused -Wall -g -std=gnu99
CCFLAGS = $(CFLAGS)
LIBS    = -lpthread

EXECS = exemple

default: $(EXECS)

$(EXECS): %: %.o dsm.o common.o

#%.o:%.c
#	$(CC) $(CFLAGS) -c $<

%:%.o dsm.o
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-mv $@ ../Phase1/bin/		

common.o : common.c common_impl.h
	$(CLINKER) $(CCFLAGS) -o common.o -c common.c

clean:
	@-/bin/rm -f *.o *~ PI* $(EXECS) *.out core  

