#! /bin/bash

args=("$@")
currentdir=$(pwd)

sed "s#{{STARTING_DIR}}#$currentdir#g" $1 | \
    sed "s#{{HOME_DIR}}#$HOME#g" > test_temp.org

./testy test_temp.org ${args[@]:1}
rm -f test_temp.org
