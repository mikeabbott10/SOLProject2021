#!/bin/bash

# 1st client
#	writing a large file in order to get evicted files back
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-W test/testFolder/almost1mbfile.txt -t200 -p

echo ------------------------------------------------------------------------------------

# 2nd client:
#	writing every file inside test/testFolder/folder1 and subdirectories. This will lead to eviction due to
#		size capacity. Going to find test/testFolder/almost1mbfile.txt inside test/testFolder/evictedFiles dir
#	(error) -> writing an existing file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-w test/testFolder/folder1,0 -W test/testFolder/folder1/chromeicon.png \
	-D test/testFolder/evictedFiles -t200 -p

# 3rd client
#	reading and storing a file if not evicted
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-r test/testFolder/folder1/chromeicon.png -d test/testFolder/folder2 -t200 -p

echo ------------------------------------------------------------------------------------

# 4th client 
#	(error) removing a not locked-by-me file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-c test/testFolder/folder1/chromeicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 5th client
#	read 70 random files storing them in ./test/testFolder/folder2 and locking and removing a file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-R70 -d test/testFolder/folder2 -l test/testFolder/folder1/operaicon.png -c test/testFolder/folder1/operaicon.png -t200 -p