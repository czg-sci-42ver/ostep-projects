#! /bin/bash
# run init_vm_xv6.sh first
PROJECT_DIR=vm-xv6-intro
cp -r ~/xv6-public/* ~/ostep-projects/${PROJECT_DIR}/src
../tester/run-tests.sh $*
