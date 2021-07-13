# The current absolute path (please run make from ./Server directory)
CURDIR = ${PWD}
# RUN bash script
RUN = run.sh valgrind_run.sh

# compilatore da usare
CC		=  gcc
# aggiungo alcuni flags di compilazione
CCFLAGS	        += -std=c99 -Wall -Werror
# gli includes sono nella directory includes
INCDIR          = ./includes
# cerco gli include oltre che in INCDIR anche nella dir. corrente
INCLUDES	= -I $(INCDIR)
# librerie da linkare a tutti i files (es. -lm per le librerie matematiche)
LDFLAGS 	= -pthread -lm
# altre librerie  (es. -L<path> -lmylib)
LIBS1		= -L "./mylibs" -lgeneral_utility -lconn -lcmdlineparser -lclient_API
# flags di ottimizzazione 
OPTFLAGS	= -O3
# flags di debugging
DBGFLAGS        = -g
FLAGS           = $(DBGFLAGS)
ARFLAGS         = rvs

TARGETS		= bin/client

# ld.so understands the string $ORIGIN (or equivalently ${ORIGIN}) in 
# an rpath specification to mean the directory containing the application 
# executable. Thus, an application located in somedir/app could be 
# compiled with gcc -Wl,-rpath,'$ORI‐ GIN/../lib' so that it finds an 
# associated shared library in somedir/lib no matter where somedir is 
# located in the directory hierarchy.


.PHONY: all clean cleanall

# root target
all		: $(TARGETS)

bin/client: src/main.c mylibs/libgeneral_utility.so mylibs/libconn.so mylibs/libcmdlineparser.so mylibs/libclient_API.so mylibs/libclient_API.so
	$(CC) $(INCLUDES) $(CCFLAGS) $(FLAGS) $< -o $@ -Wl,-rpath='$$ORIGIN/../mylibs' $(LDFLAGS) $(LIBS1)
	
mylibs/libclient_API.so: objs/client_API.o $(INCDIR)/client_API.h mylibs/libgeneral_utility.so mylibs/libconn.so
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../mylibs' -L "./mylibs" -lgeneral_utility -lconn

mylibs/libconn.so: objs/conn.o $(INCDIR)/conn.h mylibs/libgeneral_utility.so
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../mylibs' -L "./mylibs" -lgeneral_utility

mylibs/libcmdlineparser.so: objs/cmdLineParser.o $(INCDIR)/cmdLineParser.h mylibs/libgeneral_utility.so
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../mylibs' -L "./mylibs" -lgeneral_utility

mylibs/libgeneral_utility.so: objs/general_utility.o $(INCDIR)/general_utility.h
	$(CC) -shared -o $@ $< $(LDFLAGS)



objs/%.o: src/%.c
	$(CC) -fPIC $(INCLUDES) $(CCFLAGS) $< -c -o $@ $(LDFLAGS)

clean		: 
	-rm -f $(TARGETS) $(RUN)
cleanall	: clean
	-rm -f $(TARGETS) $(RUN) *~ objs/* mylibs/*