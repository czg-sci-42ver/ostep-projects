#! /bin/bash
# cp ../initial-xv6/test-getreadcount.sh .
# borrow from ../initial-xv6/init_xv6.sh
cd ~/xv6-public
git clean -f -x
git reset --hard origin/master
PATCH_FILE=xv6.patch
PROJECT_DIR=initial-xv6-tracer
cp ~/ostep-projects/${PROJECT_DIR}/patch/${PATCH_FILE} .
git apply ${PATCH_FILE}
rm ${PATCH_FILE}
##
cd ~/ostep-projects/${PROJECT_DIR}
# cp -r ../initial-xv6/tests .
rm -r tests
rm -r tests-out
rm -r src
mkdir src
mkdir tests
cp ~/xv6-public/* ~/ostep-projects/${PROJECT_DIR}/src
./test-getreadcount.sh
