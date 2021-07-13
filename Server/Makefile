# The current absolute path (please run make from ./Server directory)
CURDIR = ${PWD}
# RUN bash script
RUN = run.sh valgrind_run.sh

# compilatore da usare
CC		=  gcc
# aggiungo alcuni flags di compilazione
CCFLAGS	        += -std=c99 -Wall -Werror -Wno-unknown-pragmas
# gli includes sono nella directory includes
INCDIR          = ./includes
# cerco gli include oltre che in INCDIR anche nella dir. corrente
INCLUDES	= -I $(INCDIR)
# librerie da linkare a tutti i files (es. -lm per le librerie matematiche)
LDFLAGS 	= -pthread -lm
# altre librerie  (es. -L<path> -lmylib)
LIBS1		= -L "./mylibs" -lgeneral_utility -lserver_config -lfilesystem_util -lconnection_util -lshared_values_util -licl_hash
# flags di ottimizzazione 
OPTFLAGS	= -O3
# flags di debugging
DBGFLAGS        = -g
FLAGS           = $(DBGFLAGS)
ARFLAGS         = rvs

TARGETS		= bin/server

# ld.so understands the string $ORIGIN (or equivalently ${ORIGIN}) in 
# an rpath specification to mean the directory containing the application 
# executable. Thus, an application located in somedir/app could be 
# compiled with gcc -Wl,-rpath,'$ORI‐ GIN/../lib' so that it finds an 
# associated shared library in somedir/lib no matter where somedir is 
# located in the directory hierarchy.


.PHONY: all clean cleanall

# root target
all		: $(TARGETS)

bin/server: src/main.c mylibs/libgeneral_utility.so mylibs/libserver_config.so mylibs/libconnection_util.so mylibs/libfilesystem_util.so
	$(CC) $(INCLUDES) $(CCFLAGS) $(FLAGS) $< -o $@ -Wl,-rpath='$$ORIGIN/../mylibs' $(LDFLAGS) $(LIBS1)
	

mylibs/libconnection_util.so: objs/connection_util.o $(INCDIR)/connection_util.h mylibs/libgeneral_utility.so mylibs/libfilesystem_util.so mylibs/libshared_values_util.so
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../mylibs' -L "./mylibs" -lgeneral_utility -lfilesystem_util

mylibs/libfilesystem_util.so: objs/filesystem_util.o $(INCDIR)/filesystem_util.h mylibs/libgeneral_utility.so mylibs/libicl_hash.so
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../mylibs' -L "./mylibs" -lgeneral_utility -licl_hash

mylibs/libserver_config.so: objs/server_config.o $(INCDIR)/server_config.h mylibs/libgeneral_utility.so
	$(CC) -shared -o $@ $< -Wl,-rpath='$$ORIGIN/../mylibs' -L "./mylibs" -lgeneral_utility

mylibs/libgeneral_utility.so: objs/general_utility.o $(INCDIR)/general_utility.h
	$(CC) -shared -o $@ $< $(LDFLAGS)

mylibs/libshared_values_util.so: objs/shared_values_util.o $(INCDIR)/shared_values_util.h
	$(CC) -shared -o $@ $< $(LDFLAGS)

mylibs/libicl_hash.so: objs/icl_hash.o $(INCDIR)/icl_hash.h
	$(CC) -shared -o $@ $< $(LDFLAGS)

objs/%.o: src/%.c
	$(CC) -fPIC $(INCLUDES) $(CCFLAGS) $(LDFLAGS) $< -c -o $@ 

clean		: 
	-rm -f $(TARGETS) $(RUN) ../*.sk
cleanall	: clean
	-rm -f $(TARGETS) $(RUN) *~ objs/* mylibs/* ../*.sk