#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "user/syscall.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "devices/input.h"
#include "string.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "kernel/list.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

static struct lock file_lock;
void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	lock_init(&file_lock);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */

bool validation(uint64_t *ptr){
	/* ptr이 커널 영역인지 확인 (커널영역에 접근하면 안됨) */
	
	// NULL 이거나 커널 영역을 가리키는 포인터면 flase
	struct thread *t = thread_current();
	if(ptr == NULL || is_kernel_vaddr(ptr)) return false;

	if(is_kern_pte(t->pml4)){
		return false;
	}
	// if(ptr == NULL || is_kernel_vaddr(ptr)){
	// 	pml4_destroy(t->pml4);
	// 	return false;
	// }
	return true;
}

// void set_kernel_stack(struct intr_frame *f){

// }

struct file* return_file(int fd) {
	struct thread *t = thread_current();
	return t->fdt[fd];
}


void sys_halt(){
	power_off();
}

void sys_exit(int status) {
	struct thread *cur_t = thread_current();
	struct thread *parent_t = cur_t->parent;
	// parent_t->next_child = list_size(&parent_t->child_list);
	printf("%s: exit(%d)\n", cur_t->name, status);
	// printf("parent_t tid : %d\n", parent_t->tid);
	parent_t->exit_status = status;
	thread_exit();
	return;
}


int sys_exec (const char *cmd_line){
	
	if (!validation(cmd_line))
    {
        sys_exit(-1);
	}

	char *fn_copy = palloc_get_page(PAL_ZERO);
	int size = strlen(cmd_line) + 1;
	strlcpy(fn_copy, cmd_line, size);
	int result = process_exec (fn_copy);
	if (result = -1){
		return -1;
	}
}


// pid_t
// sys_fork (const char *thread_name) {
// 	if (!validation(thread_name))
//     {
//         sys_exit(-1);
// 	}
// 	struct thread *cur_t = thread_current();
// 	tid_t child = process_fork(thread_name, &(cur_t->tf));

// 	return child;
// }

pid_t
sys_fork (struct intr_frame *f) {

	if (!validation(f->R.rax))
    {
        sys_exit(-1);
	}
	tid_t child = process_fork(f->R.rdi, f);

	struct thread* t = thread_current();
	ASSERT(t->name != "idle");
	
	t->waiting_child = get_child_process(&t->child_list, child);
	//printf("포크 완료를 기다림 parent-%d wait child - %d\n",t->tid, t->waiting_child->tid);
	sema_down(&t->load_sema);
	//printf("sema up 됨 ~~~ child : %d\n", child);
	return child;
}


bool
sys_create (const char *file, unsigned initial_size) {
	
	if (!validation(file))
    {
        //printf("is not valid\n");
        sys_exit(-1);
    }

	return filesys_create(file,initial_size);
}

bool
sys_remove (const char *file) {
	if (!validation(file))
    {
       // printf("is not valid\n");
        sys_exit(-1);
    }
	lock_acquire(&file_lock);
	return filesys_remove(file);
	lock_release(&file_lock);
}

int
find_next_fd(struct thread *t) {

	int cur_fd = t->next_fd;
	while(cur_fd < 64)
	{
		if(t->fdt[cur_fd] == NULL) {
			t->next_fd = cur_fd;
			return 1;
		}
		cur_fd++;
	}

	return -1;
}


int
sys_open (const char *file) {
	if (!validation(file))
    {
        //printf("is not valid\n");
        sys_exit(-1);
    }

	struct thread* t = thread_current();

	lock_acquire(&file_lock);
	struct file *open_file = filesys_open(file);
	lock_release(&file_lock);

	/* 찾는데 실패하면 NULL 반환받음 -> return -1 */
	if (open_file == NULL) return -1;

	int cur_fd = t->next_fd;
	t->fdt[cur_fd] = open_file;

	// t->source = open_file;
	// file_deny_write(t->source);

	if(find_next_fd(t) == -1) {
		printf("파일 디스크립터 다 참^^");
		//thread_exit(0);
	}
	// printf("open fd : %d \n", cur_fd);
	return cur_fd;
}


int
sys_filesize (int fd) {
	return file_length(return_file(fd));
}

int
sys_read (int fd, void *buffer, unsigned size) {
	if (!validation(buffer))
    {
        // printf("is not valid\n");
        thread_exit();
    }
	
	int byte_size = -1;
	if(fd == 0){
		input_getc();
		byte_size = size;
	}
	/* 표준 출력 디스크립터를 읽으려고 시도 */
	else if(fd == 1){
		sys_exit(-1);
	}
	else{
		struct file *read_file = return_file(fd);

		sema_down(&(read_file->completion_wait));
		byte_size = file_read(read_file,buffer,size);
		sema_up(&(read_file->completion_wait));
	}
	/* TODO: 실패시, -1 반환 구현 예정 */	
	return byte_size;
}

int
sys_write (int fd, const void *buffer, unsigned size) {
	if (!validation(buffer))
    {
        // printf("is not valid\n");
        sys_exit(-1);
    }

	int byte_size = -1;
	/* 표준 출력 */
	if(fd == 1){
    	putbuf(buffer,size);
		byte_size = (size > strlen(buffer)) ? strlen(buffer) : size;
	}
	/* 표준 입력 디스크립터에 쓰려고 시도 */
	else if(fd == 0){
		sys_exit(-1);
	}
	else{
		struct file *write_file = return_file(fd);

		sema_down(&(write_file->completion_wait));
		byte_size = file_write(write_file, buffer,size);
		sema_up(&(write_file->completion_wait));
	}
	return byte_size;
	
}

void
sys_seek (int fd, unsigned position) {
	
	struct file *seek_file = return_file(fd);
	sema_down(&(seek_file->completion_wait));
	file_seek(seek_file,position);
	sema_up(&(seek_file->completion_wait));
}

unsigned
sys_tell (int fd) {
	return file_tell(return_file(fd));
}

void
sys_close (int fd) {
	struct file *close_file = return_file(fd);
	file_close(close_file);	
	struct thread *t = thread_current();
	t->fdt[fd] = NULL;
	file_close(return_file(fd));
	if(fd < t->next_fd)
		t->next_fd = fd;
}

int
sys_wait(pid_t pid) {

	//thread_current()->waiting_child = get_child_process(&thread_current()->child_list,pid);

	return process_wait(pid);
}


void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	if(thread_current()->name == "idle")
		return;
	uint64_t number = f->R.rax;
	
	// printf("\npid : %d \n", thread_current()->tid);
	// printf("system call number : %d\n", number);
	// printf("f->rdi : %d\n", f->R.rdi);
	// printf("f->rsi : %s\n", f->R.rsi);
	// printf("f->rdx : %d\n", f->R.rdx);
	// printf("f->r10 : %d\n", f->R.r10);
	// printf("f->r8 : %d\n", f->R.r8);
	// printf("f->r9 : %d\n", f->R.r9);
	// printf("f->rsp : %d\n", f->rsp);

	switch(number){
		case SYS_HALT:
			sys_halt();
			// set_kernel_stack(f);
			return;

		case SYS_EXIT:
			sys_exit(f->R.rdi);
			//set_kernel_stack(f);
			return;

		case SYS_FORK:
			// set_kernel_stack(f);
			// f->R.rax = sys_fork(f->R.rdi);
			f->R.rax = sys_fork(f);
			return;

		case SYS_EXEC:
			sys_exec(f->R.rdi);
			return;

		case SYS_WAIT:
			f->R.rax = sys_wait(f->R.rdi);
			// set_kernel_stack(f);
			return;
		
		case SYS_CREATE:
			// char * file_name = ;
			// unsigned initial_size = ;

            f->R.rax = sys_create(f->R.rdi,f->R.rsi);
			return;

		
		case SYS_REMOVE:
	
            f->R.rax = sys_remove( f->R.rdi);
			return;

		
		case SYS_OPEN:
			
            f->R.rax = sys_open(f->R.rdi);        
			return;

		case SYS_FILESIZE:

            f->R.rax = sys_filesize(f->R.rdi);
			return;


		case SYS_READ:
            f->R.rax = sys_read(f->R.rdi,f->R.rsi,f->R.rdx);
			return;


		case SYS_WRITE:
            f->R.rax = sys_write(f->R.rdi,f->R.rsi,f->R.rdx);
			return;


		case SYS_SEEK:
			sys_seek(f->R.rdi, f->R.rsi);
			return;


		case SYS_TELL:
            f->R.rax = sys_tell(f->R.rdi);
			return;


		case SYS_CLOSE:
            sys_close(f->R.rdi);
			return;

		default:
			break;
	}

	// printf ("system call!\n");
	thread_exit ();
}