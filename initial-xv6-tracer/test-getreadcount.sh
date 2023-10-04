#! /bin/bash

if ! [[ -d src ]]; then
    echo "The src/ dir does not exist."
    echo "Your xv6 code should be in the src/ directory"
    echo "to enable the automatic tester to work."
    exit 1
fi

# ../tester/run-tests.sh $*
# borrow from ../initial-xv6/1.run
cd src; ../../tester/run-xv6-command.exp CPUS=1 Makefile test_readcount | grep XV6_TEST_OUTPUT; cd ..

