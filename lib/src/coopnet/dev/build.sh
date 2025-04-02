#!/bin/bash
set -e  # Abort if any command returns a non-zero exit status

make clean
make -j
cp ./bin/libcoopnet.a ~/sm64ex-coop/lib/coopnet/linux/libcoopnet.a
cp common/libcoopnet.h ~/sm64ex-coop/lib/coopnet/include/libcoopnet.h
