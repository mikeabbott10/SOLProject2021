# The current absolute path (path where this Makefile is)
CURDIR = ${PWD}
# RUN bash script
RUN = run.sh

# compilatore da usare
CC		=  gcc
# aggiungo alcuni flags di compilazione
CCFLAGS	        += -std=c99 -Wall
# gli includes sono nella directory includes
INCDIR          = ./includes
# cerco gli include oltre che in INCDIR anche nella dir. corrente
INCLUDES	= -I $(INCDIR)
# librerie da linkare a tutti i files (es. -lm per le librerie matematiche)
LDFLAGS 	= -pthread
# altre librerie  (es. -L<path> -lmylib)
LIBS1		= -L "$(CURDIR)/mylibs" -lgeneral_utility -lserver_config
# flags di ottimizzazione 
OPTFLAGS	= # -O3
# flags di debugging
DBGFLAGS        = -g
FLAGS           = $(DBGFLAGS)
ARFLAGS         = rvs

TARGETS		= bin/server

.PHONY: all clean cleanall# test1 test2 test3

# root target
all		: $(TARGETS)

bin/server: src/main.c mylibs/libgeneral_utility.so mylibs/libserver_config.so
	$(CC) $(INCLUDES) $(CCFLAGS) $(FLAGS) $< -o $@ -Wl,-rpath="$(CURDIR)/mylibs" $(LDFLAGS) $(LIBS1)
	echo "cd $(CURDIR)\nvalgrind --leak-check=full ./bin/server" > run.sh

mylibs/libserver_config.so: objs/server_config.o $(INCDIR)/server_config.h mylibs/libgeneral_utility.so
	$(CC) -shared -o $@ $< -Wl,-rpath="$(CURDIR)/mylibs" -L "$(CURDIR)/mylibs" -lgeneral_utility

mylibs/libgeneral_utility.so: objs/general_utility.o $(INCDIR)/general_utility.h
	$(CC) -shared -o $@ $<

objs/general_utility.o: src/general_utility.c $(INCDIR)/general_utility.h
objs/server_config.o: src/server_config.c $(INCDIR)/server_config.h
objs/general_utility.o objs/server_config.o:
	$(CC) -fPIC $(INCLUDES) $(CCFLAGS) $< -c -o $@ 


clean		: 
	-rm -f $(TARGETS) $(RUN)
cleanall	: clean
	-rm -f $(TARGETS) $(RUN) *~ objs/* mylibs/*