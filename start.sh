#!/bin/bash

COMMAND="./server/drone -c 0x0f -n 2"

if [ $1 ]
  then
    COMMAND="$COMMAND -- -p $1"
fi

export LD_LIBRARY_PATH=dpdk-1.7.0/x86_64-native-linuxapp-gcc/lib/:dpdkadapter/

eval $COMMAND

#gdb --args ./server/drone -c 0x07 -n 2
