#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/fixedpoint.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* decay */
#define DECAY X_DIVIDE_Y(X_MULTIPLY_N(load_avg, 2), X_ADD_N(X_MULTIPLY_N(load_avg, 2), 1))

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */

struct thread {
	/* 상태 머신을 위한 스레드 구조체 정의 */
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	int64_t wakeup_time;				/* alarm clock에 의해 깨어날 시간 */
	
	/* 임계구역 접근을 위해 추가 */
	struct lock *wait_on_lock;

	/* Priority Donation 구현을 위해 추가 */
	int original_priority;
	struct list_elem delem;
	struct list donations;

	/* 4.4BSD 구현을 위해 추가 */
	int nice;
	int recent_cpu;
	struct list_elem assemble_elem;


#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
	struct file* fdt[64];               /* file descriptor table 추가 */
	int next_fd;                        /* 가용한 다음 fd */
	struct intr_frame parent_if;

	struct file* source;				/* 프로세스 실행에 사용한 실행파일 */
	struct thread *parent;
  
	struct list child_list;
	struct list_elem child_elem;
	// int next_child;
	

	struct thread *waiting_child;
	struct semaphore exit_sema;
	struct semaphore load_sema;
	struct semaphore parent_wait_sema;

	int exit_status;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;
// extern int load_avg;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

void thread_sleep(int64_t ticks);
int64_t thread_wakeup(int64_t ticks);
void thread_print_list(struct list *list);
void thread_print_readylist();
void thread_print_list_bysema(struct list *list);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
void context_switch (void);

void do_iret (struct intr_frame *tf);

void mlfq_priority_update();
void mlfq_recent_cpu_update();

bool less_function(const struct list_elem *a, const struct list_elem *b, void *aux);
bool high_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool high_priority_donation(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool high_priority_sema(struct list_elem *a, struct list_elem *b, void *aux UNUSED);
bool high_priority_donation(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
#endif /* threads/thread.h */