#! /bin/bash
cd ~/xv6-public
git clean -f -x
git reset --hard origin/master
cp ~/ostep-hw/projects/initial-xv6/readcount_mod.patch .
git apply readcount_mod.patch
# finish init ~/xv6-public
cd ~/ostep-projects/initial-xv6
rm -r tests-out
rm -r ~/ostep-projects/initial-xv6/src
mkdir ~/ostep-projects/initial-xv6/src
cp ~/xv6-public/* ~/ostep-projects/initial-xv6/src
./test-getreadcount.sh