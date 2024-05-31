#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "user/syscall.h"
#include "threads/thread.h"

void syscall_init(void);

bool validation(uint64_t *ptr);
void set_kernel_stack(struct intr_frame *f);
struct file* return_file(int fd) ;
int find_next_fd(struct thread *t) ;
void sys_halt();
void sys_exit(int status) ;
int sys_exec (const char *cmd_line);
pid_t sys_fork (struct intr_frame *f);
// pid_t sys_fork (const char *thread_name);
bool sys_create (const char *file, unsigned initial_size) ;
bool sys_remove (const char *file) ;
int sys_open (const char *file) ;
int sys_filesize (int fd) ;
int sys_read (int fd, void *buffer, unsigned size) ;
int sys_write (int fd, const void *buffer, unsigned size) ;
void sys_seek (int fd, unsigned position) ;
unsigned sys_tell (int fd) ;
void sys_close (int fd) ;

#endif /* userprog/syscall.h */
