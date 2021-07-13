#!/bin/bash

# 1st client:
#	writing every file inside ./test/folder1 and subdirectories
#	(error) -> writing an existing file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-w test/folder1,0 -W test/folder1/chromeicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 2nd client
#	locking file with success
#	(error) -> unlocking an unlocked file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-l test/folder1/chromeicon.png -u test/folder1/operaicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 3rd client
#	reading and storing a file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-r test/folder1/chromeicon.png -d test/folder2 -t200 -p

echo ------------------------------------------------------------------------------------

# 4th client 
#	(error) removing a locked-by-someone-else file
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-c test/folder1/chromeicon.png -t200 -p

echo ------------------------------------------------------------------------------------

# 5th client
#	locking and removing a file and read 70 random files storing them in ./test/testFolder/folder2
valgrind --leak-check=full ./Client/bin/client -f ./LSOfiletorage.sk \
	-l test/folder1/operaicon.png -c test/folder1/operaicon.png -R70 -d -d test/testFolder/folder2 -t200 -p
