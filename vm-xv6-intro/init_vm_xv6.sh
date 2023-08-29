#! /bin/bash
xv6_pre(){
    cd ~/xv6-public
    git clean -f -x
    git reset --hard origin/master
}
# try without patch
xv6_pre
cp ~/ostep-hw/projects/xv6_install.patch ./xv6_install.patch
git apply xv6_install.patch
# finish init ~/xv6-public
PROJECT_DIR=vm-xv6-intro
# init project test
cp -r ~/ostep-hw/projects/${PROJECT_DIR}/* ~/ostep-projects/${PROJECT_DIR}
init_src(){
    cd ~/ostep-projects/$1
    if [[ -d tests-out ]] ;then rm -r tests-out;fi
    if [[ -d ~/ostep-projects/$1/src ]] ;then rm -r ~/ostep-projects/$1/src;fi
    mkdir ~/ostep-projects/$1/src
    cp -r ~/xv6-public/* ~/ostep-projects/$1/src
}
init_src ${PROJECT_DIR}
# run test
# ../tester/run-tests.sh $*
test_src(){
    init_src $1
    ../tester/run-tests.sh $*
}

# try with patch
xv6_pre
FILE=not-map-null.patch
use_project_patch(){
    cp ~/ostep-hw/projects/$1/$2 .
    # use `sed 's/x86/i386-elf-/g;p' ${FILE} | less` to test whether it works.
    # notice the `-e 'p'` https://stackoverflow.com/a/25306308/21294350 -> it is needed
    # replace in vscode "^([A-Z#])" -> " $1" to keep the tab except when starting with "-/+"
    sed -i 's/x86_64-elf-/i386-elf-/g' $2
    sed -i -e '14i@@ -76,7 +76,7 @@ AS = $(TOOLPREFIX)gas\
 LD = $(TOOLPREFIX)ld\
 OBJCOPY = $(TOOLPREFIX)objcopy\
 OBJDUMP = $(TOOLPREFIX)objdump\
-CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer\
+CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -fno-omit-frame-pointer\
 CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)\
 ASFLAGS = -m32 -gdwarf-2 -Wa,-divide\
 # FreeBSD ld wants ``elf_i386_fbsd''
    ' $2
    git apply $2
}
# `use_project_patch` No use. It may be combined with other patches -> 
# "@@ -72,11 +72,11 @@ QEMU = $(shell if which qemu > /dev/null; \" ...
# use_project_patch ${PROJECT_DIR} ${FILE}

FILE=not-map-null_mod.patch
direct_apply_patch(){
    xv6_pre
    cp ~/ostep-hw/projects/$1/$2 .
    git apply $2
}
echo ${FILE}
# direct_apply_patch ${PROJECT_DIR} ${FILE}
# FILE=protect-my-balls_mod.patch
FILE=protect-my-balls_use_gas_mod.patch
direct_apply_patch ${PROJECT_DIR} ${FILE}

test_src ${PROJECT_DIR}