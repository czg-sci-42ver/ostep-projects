#include <stdio.h>
#include "udp.h"
#include "../mfs.h"

void check_rc(int rc) {
  if (rc == -1) {
    exit(EXIT_FAILURE);
  }
}

void error_log() { fprintf(stderr, "The following one should fail\n"); }

// client code
int main(int argc, char *argv[]) {
  /*
  1. send to localhost:10000
  2. function as part of "MFS_Init".
  */
  int rc = MFS_Init("localhost", 10000);
  assert(rc != -1);

  rc = MFS_Lookup(ROOT_INUM, ".");
  check_rc(rc);

  MFS_Stat_t file_stat;
  rc = MFS_Stat(ROOT_INUM, &file_stat);
  check_rc(rc);
  /*
  0 -> checkpoint
  1 -> root data
  2 -> root inode and related imap
  */
  uint end_block = 2;

  uint inum = ROOT_INUM;
  /*
  Since the following `rc = MFS_Unlink(ROOT_INUM, "foo");` unlinks the file,
  when rerun this `MFS_Creat`, the file will be written after the checkpoint
  log_end, so the following `MFS_Write(inum, data_to_send, end_block + 1);` will
  throw error because it writes to one wrong block address.
  */
  rc = MFS_Creat(ROOT_INUM, REGULAR_FILE, "foo");
  inum++;
  /*
  3 -> new root data
  4 -> file inode, root inode and related imap
  */
  end_block += 2;
  check_rc(rc);

  rc = MFS_Stat(ROOT_INUM, &file_stat);
  check_rc(rc);

  char data_to_send[BSIZE] = {0};
  sprintf(data_to_send, "test first\n");
#ifdef TEST_FAIL_WRITE
  error_log();
  rc = MFS_Write(inum, data_to_send, 10); // should fail
#endif
  rc = MFS_Write(inum, data_to_send, end_block + 1);
  /*
  5-> file data
  6 -> file inode and related imap
  */
  end_block += 2;
  check_rc(rc);

  memset(data_to_send, 0, BSIZE);
#ifdef TEST_FAIL_WRITE
  /*
  should fail because I write to the consecutive block after Checkpoint->log_end
  */
  error_log();
  rc = MFS_Read(inum, data_to_send, 10);
#endif
  rc = MFS_Read(inum, data_to_send, end_block - 1);
  check_rc(rc);

  rc = MFS_Creat(ROOT_INUM, DIRECTORY, "bar");
  inum++;
  /*
  7-> new dir data
  8 -> root data
  9 -> new dir inode, root inode and related imap
  */
  end_block += 3;
  check_rc(rc);

  rc = MFS_Creat(inum, REGULAR_FILE, "foo");
  inum++;
  /*
  10-> new root data
  11(0xb)-> file inode, pdir (parent dir) inode and related imap
  */
  end_block += 2;
  check_rc(rc);

  memset(data_to_send, 0, BSIZE);
  sprintf(data_to_send, "test second\n");
  rc = MFS_Write(inum, data_to_send, end_block + 1);
  /*
  12-> file data
  13(0xd)-> file inode and related imap
  */
  end_block += 2;
  check_rc(rc);

  memset(data_to_send, 0, BSIZE);
  rc = MFS_Read(inum, data_to_send, end_block - 1);
  check_rc(rc);

  rc = MFS_Lookup(inum - 1, "foo");
  check_rc(rc);

  rc = MFS_Stat(inum, &file_stat); // /bar/foo
  check_rc(rc);

  error_log();
  rc = MFS_Unlink(ROOT_INUM, "bar");

  /*
  14(0xe)-> new pdir data block
  15(0xf)-> pdir inode and imaps related with pdir and the file (maybe only one
  imap if they are same imap.)
  */
  rc = MFS_Unlink(ROOT_INUM, "foo");
  end_block += 2;
  check_rc(rc);

  error_log();
  rc = MFS_Lookup(ROOT_INUM, "foo");

  /*
  1. foo bar ...
  https://en.wikipedia.org/wiki/Metasyntactic_variable#General_usage
  2. this should reuse the invalid inum of foo -> 1.
  */
  rc = MFS_Creat(ROOT_INUM, REGULAR_FILE, "baz");
  /*
  16(0x10)-> new root data
  17(0x11)-> file inode, root inode and related imap
  */
  end_block += 2;
  check_rc(rc);

  memset(data_to_send, 0, BSIZE);
  sprintf(data_to_send, "test third\n");
  rc = MFS_Write(inum, data_to_send, end_block + 1);
  /*
  18(0x12)-> file data
  19(0x13)-> file inode and related imap
  */
  end_block += 2;
  check_rc(rc);

  memset(data_to_send, 0, BSIZE);
  rc = MFS_Read(inum, data_to_send, end_block - 1);
  check_rc(rc);

  rc = MFS_Lookup(ROOT_INUM, "baz");
  check_rc(rc);

  /*
  20(0x14)-> new pdir data block
  21(0x15)-> pdir inode and imaps related with pdir and the file (maybe only one
  imap if they are same imap.)
  */
  rc = MFS_Unlink(inum - 1, "foo");
  end_block += 2;
  check_rc(rc);

  /*
  1. 22(0x16)-> new pdir data block
  23(0x17)-> pdir inode and imaps related with pdir and the file (maybe only one
  imap if they are same imap.)
  2. See server.c "so pinode size doesn't ..." this doesn't modify pinode size
  property.
  */
  rc = MFS_Unlink(ROOT_INUM, "bar");
  end_block += 2;
  check_rc(rc);

  rc = MFS_Stat(ROOT_INUM, &file_stat);
  check_rc(rc);

  rc = MFS_Shutdown();
  check_rc(rc);

  return 0;
}
