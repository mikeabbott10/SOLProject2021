#!/bin/bash

echo -------------CLIENTS---------------------------------------------------------------

# 1st client:
#	writing every file inside test/testFolder/folder1 and subdirectories. This will lead to eviction due to
#		file capacity. No file gets back to the client because the eviction is performed after an openFile
#		request. Evicted files get back to the client after a writeFile only: see test2_2.
#	(error) -> writing an existing file
./Client/bin/client -f ./LSOfiletorage.sk \
	-w test/testFolder/folder1,0 -W test/testFolder/folder1/chromeicon.png \
	-D test/testFolder/evictedFiles -t200 -p

echo ------------------------------------------------------------------------------------

# 2nd client
#	locking file with success if not evicted 
#	(error) -> unlocking an unlocked file
./Client/bin/client -f ./LSOfiletorage.sk \
	-l test/testFolder/folder1/chromeicon.png -u test/testFolder/folder1/operaicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 3rd client
#	reading and storing a file
./Client/bin/client -f ./LSOfiletorage.sk \
	-r test/testFolder/folder1/chromeicon.png -d test/testFolder/folder2 -t200 -p

echo ------------------------------------------------------------------------------------

# 4th client 
#	(error) removing a not locked-by-me file
./Client/bin/client -f ./LSOfiletorage.sk \
	-c test/testFolder/folder1/chromeicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 5th client
#	read 70 random files storing them in ./test/testFolder/folder2 and locking and removing a file
./Client/bin/client -f ./LSOfiletorage.sk \
	-R70 -d test/testFolder/folder2 -l test/testFolder/folder1/operaicon.png -c test/testFolder/folder1/operaicon.png -t200 -p