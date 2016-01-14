#!/bin/bash -x

rm -f 1hs 1hc
rm -f 2hs 2hc
rm -f 3hs 3hc 

./sp -r $1 -s 4 -d 4 
mv tree* ../realrun/
t1=""
t2=""
t3=""

if [ -f 1hs ] 
then
    t1="0"
fi

if [ -f 1hc ] 
then
    t1="1"
fi

if [ -f 2hs ] 
then
    t2="0"
fi

if [ -f 2hc ] 
then
    t2="1"
fi

if [ -f 3hs ] 
then
    t3="0"
fi

if [ -f 3hc ] 
then
    t3="1"
fi


cd ../realrun/
./cc.sh $t1 $t2 $t3
./realpc -r $1 -t $2 -l tree0 -m tree1 -n tree2
