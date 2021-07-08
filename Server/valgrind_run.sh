#!/bin/bash
cd /home/lorenzo/Documenti/Università/Progetto/Progetto2021/Server
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/server
