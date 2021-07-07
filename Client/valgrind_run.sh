#!/bin/bash
cd /home/lorenzo/Documenti/Università/Progetto/Progetto2021/Client
valgrind --leak-check=full ./bin/client -f ../LSOfiletorage.sk -w w_dirnome,98 -W W_fail,W_faildue,W_failtre -D D_dirnomignolo -r r_leggoquesto,r_leggopurequesto -d d_cartellona -l l_file1,l_file2,l_file3,l_file4 -u u_file1,u_file2,u_file3,u_file4,u_file5 -c c_file1,c_file2 -l 'CIAOOOO FILE.TXT' -l OPPP -c BLUUU -l SIE -t500
