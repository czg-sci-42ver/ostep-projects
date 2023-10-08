CC     := gcc
CFLAGS := -Wall -Werror -g

SRCS   := client.c \
	server.c 

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

LIB=../libmfs.so
.PHONY: all
all: ${LIB} ${PROGS} 
HEADERS=../mfs.h ../mfs_helper.h ./udp.h
${LIB}: ../mfs.c ./udp.c ${HEADERS}
	make -C ..
ORIGIN_LD=${LD_LIBRARY_PATH}
# export LD_LIBRARY_PATH=${ORIGIN_LD}:
${PROGS} : % : %.o Makefile
# https://stackoverflow.com/a/8426110/21294350
# https://stackoverflow.com/a/15727498/21294350 see `man ld`
	$(shell if [[ -f fs.img ]];then rm fs.img;fi)
	${CC} ${CFLAGS} $< -o $@ udp.c -L$(shell dirname "${PWD}") -l:libmfs.so

clean:
	rm -f ${PROGS} ${OBJS}

%.o: %.c Makefile ${LIB}
	${CC} ${CFLAGS} -c $<
	$(shell clang-format -i *.[ch] ../*.[ch])
