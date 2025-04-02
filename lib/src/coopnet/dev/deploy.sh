#!/bin/bash
set -e  # Abort if any command returns a non-zero exit status

make clean
make LOGGING=1 -j
cp ./bin/libcoopnet.a ~/sm64ex-coop/lib/coopnet/linux/libcoopnet.a
cp common/libcoopnet.h ~/sm64ex-coop/lib/coopnet/include/libcoopnet.h
ssh root@143.244.214.139 "systemctl stop coopnet"
scp ./bin/server coopnet@143.244.214.139:/coopnet/server
ssh root@143.244.214.139 "systemctl restart coopnet"
