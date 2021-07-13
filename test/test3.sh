#!/bin/bash

clients=(
    
    )

while true 
do
    i=$(( RANDOM % ${#clients[@]}))
    ${clients[i]}
done