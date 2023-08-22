#! /bin/bash
# use alias https://askubuntu.com/a/98786/1682447
echo_e="echo -e";
for i in {1..22};do
$echo_e '------\n' $i;cat ~/ostep-projects/processes-shell/tests/$i.desc;cat ~/ostep-projects/processes-shell/tests/$i.in;
echo "run:";cat ~/ostep-projects/processes-shell/tests/$i.run;
echo "out:";cat ~/ostep-projects/processes-shell/tests/$i.out;RED='\033[0;31m';
NC='\033[0m';
echo "err:";
# pipe the cat https://stackoverflow.com/a/6779351/21294350 from https://unix.stackexchange.com/questions/33858/bash-read-command-and-stdin-redirection#comment45920_33858
# use the command group in bash https://stackoverflow.com/a/46093063/21294350
cat ~/ostep-projects/processes-shell/tests/$i.err | (read test; $echo_e "${RED}${test}");$echo_e -n "${NC}";done | less -R