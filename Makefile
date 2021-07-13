SERVERPID = ../server.PID

.PHONY: all clean cleanall test1 test2_1 test2_2 test3
.SILENT: test1 test2_1 test2_2 cleanall

all:
	(cd Server && make all)
	(cd Client && make all)

clean:
	(cd Server && make clean)
	(cd Client && make clean)

cleanall:
	(cd Server && make cleanall)
	(cd Client && make cleanall)
	if [ -e server.PID ]; then \
	    rm server.PID || true;\
	fi;
	rm test/testFolder/folder2/* || true;
	rm test/testFolder/evictedFiles/* || true;

test1: test/test1.sh Client/bin/client Server/bin/server
	echo "Writing server configuration file for test 1."
	echo "file_capacity=10000\n\
	byte_capacity=134217728\n\
	worker_threads=1\n\
	max_connections_buffer_size=500\n\
	socket_path=../LSOfiletorage.sk\n\
	eviction_policy=0\n\
	#end_of_config" > ./Server/src/config.txt
	(cd Server && ((valgrind --leak-check=full bin/server > ../log.txt 2>&1) & echo $$! > $(SERVERPID) &) )
	./test/test1.sh
	if [ -e server.PID ]; then \
	    kill -1 $$(cat server.PID) || true; \
	fi;

test2_1: test/test2_1.sh Client/bin/client Server/bin/server
	echo "Writing server configuration file for test 2_1."
	echo "file_capacity=10\n\
	byte_capacity=1048576\n\
	worker_threads=4\n\
	max_connections_buffer_size=500\n\
	socket_path=../LSOfiletorage.sk\n\
	eviction_policy=1\n\
	#end_of_config" > ./Server/src/config.txt
	(cd Server && ((valgrind --leak-check=full bin/server > ../log.txt 2>&1) & echo $$! > $(SERVERPID) &) )
	./test/test2_1.sh
	if [ -e server.PID ]; then \
	    kill -1 $$(cat server.PID) || true; \
	fi;

test2_2: test/test2_2.sh Client/bin/client Server/bin/server
	echo "Writing server configuration file for test 2_2."
	echo "file_capacity=10\n\
	byte_capacity=1048576\n\
	worker_threads=4\n\
	max_connections_buffer_size=500\n\
	socket_path=../LSOfiletorage.sk\n\
	eviction_policy=0\n\
	#end_of_config" > ./Server/src/config.txt
	(cd Server && ((valgrind --leak-check=full bin/server > ../log.txt 2>&1) & echo $$! > $(SERVERPID) &) )
	./test/test2_2.sh
	if [ -e server.PID ]; then \
	    kill -1 $$(cat server.PID) || true; \
	fi;

Client/bin/client Server/bin/server:
	make all