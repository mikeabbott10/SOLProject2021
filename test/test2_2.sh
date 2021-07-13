#!/bin/bash

echo -------------CLIENTS---------------------------------------------------------------

# 1st client
#	writing a large file in order to get evicted files back
./Client/bin/client -f ./LSOfiletorage.sk \
	-W test/testFolder/almost1mbfile.txt -t200 -p

echo ------------------------------------------------------------------------------------

# 2nd client:
#	writing every file inside test/testFolder/folder1 and subdirectories. This will lead to eviction due to
#		bytes size capacity. Going to find test/testFolder/almost1mbfile.txt inside test/testFolder/evictedFiles dir
#	(error) -> writing an existing file
./Client/bin/client -f ./LSOfiletorage.sk \
	-w test/testFolder/folder1,0 -W test/testFolder/folder1/chromeicon.png \
	-D test/testFolder/evictedFiles -t200 -p

echo ------------------------------------------------------------------------------------

# 3rd client
#	reading and storing a file if not evicted
./Client/bin/client -f ./LSOfiletorage.sk \
	-r test/testFolder/folder1/chromeicon.png -d test/testFolder/folder2 -t200 -p

echo ------------------------------------------------------------------------------------

# 4th client 
#	locking and unlocking the same file
./Client/bin/client -f ./LSOfiletorage.sk \
	-l test/testFolder/folder1/chromeicon.png -u test/testFolder/folder1/chromeicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 5th client
#	read all the files storing them in ./test/testFolder/folder2 and locking and removing a file after explicit lock (double lock doesn't fail)
./Client/bin/client -f ./LSOfiletorage.sk \
	-R0 -d test/testFolder/folder2 -l test/testFolder/folder1/operaicon.png -c test/testFolder/folder1/operaicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 6th client
#	reading and (error) storing a file
./Client/bin/client -f ./LSOfiletorage.sk \
	-r test/testFolder/folder1/chromeicon.png -d test/testFolder/folder654654 -t200 -p

echo ------------------------------------------------------------------------------------
