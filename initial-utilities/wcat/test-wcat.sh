#! /bin/bash

if ! [[ -x wcat ]]; then
    echo "wcat executable does not exist"
    exit 1
fi

echo $*

../../tester/run-tests.sh $*


