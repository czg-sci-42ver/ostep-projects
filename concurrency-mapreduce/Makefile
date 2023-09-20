all: hash_orig.out hash_orig_mod.out mapreduce.out
%.out: %.c
	$(CC) $(CFLAGS) -o $@ $<
mapreduce.out: sort.c mapreduce.c hash.c
	gcc sort.c mapreduce.c hash.c -o mapreduce.out -g -Wall -Werror
