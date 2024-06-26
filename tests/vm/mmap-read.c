/* Uses a memory mapping to read a file. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"
#include <stdio.h>

void
test_main (void)
{
  char *actual = (char *) 0x10000000;
  int handle;
  void *map;
  size_t i;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  // printf("handle : %d\n",handle);
  CHECK ((map = mmap (actual, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");
  // printf("map : %p\n",map);

  /* Check that data is correct. */
  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");
  // printf("actual : %s\n",actual);
  /* Verify that data is followed by zeros. */
  for (i = strlen (sample); i < 4096; i++)
    if (actual[i] != 0)
      fail ("byte %zu of mmap'd region has value %02hhx (should be 0)",
            i, actual[i]);
  // printf("11map : %p\n",map);
  munmap (map);
  close (handle);
}
