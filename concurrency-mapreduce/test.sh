#! /bin/bash
# https://askubuntu.com/a/98786/1682447
shopt -s expand_aliases
echo "${HOME}/.bashrc"
########
# 1. https://askubuntu.com/questions/98782/how-to-run-an-alias-in-a-shell-script#comment835166_98786
# not use .bashrc because sourced bash_aliases in it will fail here 
# 2. better to use functions https://askubuntu.com/questions/98782/how-to-run-an-alias-in-a-shell-script#comment2210159_98786
########
source ${HOME}/.bash_aliases # https://askubuntu.com/questions/98782/how-to-run-an-alias-in-a-shell-script#comment2210159_98786
make --always-make
for i in $(seq 16);do echo $(seq $i) > test_files/$i.txt;done
for i in $(seq 16);do index=$(python -c "print(17-${i})");file_list+="./test_files/$index.txt ";done;valgrind_s ./mapreduce.out ${file_list}
file_list="";
for i in $(seq 16);do index=$i;file_list+="./test_files/$index.txt ";done;valgrind_s ./mapreduce.out ${file_list}
[ -z "$PS1" ] && echo "non-interactive execution may fail with 'source ~/.bashrc'"