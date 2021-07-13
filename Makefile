.PHONY: all clean cleanall test1 test2 test3
.SILENT: test1

all:
	(cd Server && make all)
	(cd Client && make all)

clean:
	(cd Server && make clean)
	(cd Client && make clean)

cleanall:
	(cd Server && make cleanall)
	(cd Client && make cleanall)

test1: test/test1.sh Client/bin/client Server/bin/server
	echo "Writing server configuration file for test 1."
	echo "file_capacity=10000\n\
	byte_capacity=134217728\n\
	worker_threads=1\n\
	max_connections_buffer_size=500\n\
	socket_path=../LSOfiletorage.sk\n\
	eviction_policy=0\n\
	#end_of_config" > ./Server/src/config.txt
	(cd Server && (valgrind --leak-check=full bin/server > ../log.txt 2>&1 &))
	./test/test1.sh

test2: test/test2.sh Client/bin/client Server/bin/server
	./test/test2.sh

test3: test/test3.sh Client/bin/client Server/bin/server
	./test/test3.sh

Client/bin/client Server/bin/server:
	make all

