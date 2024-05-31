/* Forks and waits for a single child process. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "threads/thread.h"

void
test_main (void) 
{
  int pid;

  if ((pid = fork("child"))){
    // printf("wwww1\n");
    int status = wait (pid);
    // printf("wwww2\n");
    msg ("Parent: child exit status is %d", status);
  } else {
    msg ("child run");
    exit(81);
  }
}
