/* Verifies that memory mappings at address 0 are disallowed. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include <stdio.h>

void
test_main (void) 
{
  int handle;
  void *map;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  // map = mmap (NULL, 4096, 0, handle, 0);
  // printf("map : %p\n",map);
  CHECK (mmap (NULL, 4096, 0, handle, 0) == MAP_FAILED, "try to mmap at address 0");

}

