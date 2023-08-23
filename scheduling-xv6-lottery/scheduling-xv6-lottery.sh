#! /bin/bash
cd ~/xv6-public
git clean -f -x
git reset --hard origin/master
cp ~/ostep-hw/projects/scheduling-xv6-lottery/src/* .;
# make qemu-nox;
# finish init ~/xv6-public;
TARGET_DIR=scheduling-xv6-lottery;
cd ~/ostep-projects/${TARGET_DIR}
if [[ -d tests-out ]];then rm -r tests-out;fi
rm -r ~/ostep-projects/${TARGET_DIR}/src
mkdir ~/ostep-projects/${TARGET_DIR}/src
cp -r ~/ostep-hw/projects/scheduling-xv6-lottery/* ~/ostep-projects/${TARGET_DIR}
cp -r ~/xv6-public/* ~/ostep-projects/${TARGET_DIR}/src
./test-scheduler.sh