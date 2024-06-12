/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "vm/uninit.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "lib/string.h"
#include "vm/anon.h"

#define MAX_STACK (USER_STACK - (1 << 20))

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
	list_init(&frame_table);
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	// uninit일 경우 해당 페이지가 바뀔 type을 리턴
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 대기중인 페이지 객체를 초기화*/
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		bool is_succ = false;
		switch (VM_TYPE(type))
		{
			// case VM_UNINIT:
			// 	is_succ = uninit_initialize(page, upage);
			// 	break;
			// 	uninit_new(page, upage,init, VM_UNINIT,aux, is_succ);

		case VM_ANON:
			uninit_new(page, upage, init, type, aux, anon_initializer);
			break;

		case VM_FILE:
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
			break;

		default:
			break;
		}
		page->writable = writable;
		page->frame = NULL;
		page->st_addr = 0;
		lock_init(&page->hash_lock);
		page->cur_thread = thread_current();
		return spt_insert_page(spt, page);
	}
err:
	return false;
}
/* Find VA from spt and return page. On error, return NULL. */

/* 보충 페이지 테이블에서 va를 찾아 페이지를 반환 (error일 경우 null return) */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{

	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;

	if (va == NULL)
		return NULL;

	page->va = pg_round_down(va); // 해당 페이지의 첫번째 주소값 (offset == 0)

	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */

/* 페이지가 유효한지 검하하고 spt 테이블에 페이지를 삽입한다. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	// 가상주소가 존재하는지 검사 (추가 구현 사항)

	lock_acquire(&page->hash_lock);
	struct hash_elem *e = hash_insert(&spt->spt_hash, &page->hash_elem);
	// printf("[spt_insert_page]====\n");
	lock_release(&page->hash_lock);

	return e == NULL ? true : false;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
}

/* Get the struct frame, that
will be evicted. */
static struct frame *
vm_get_victim(void)
{
	/* TODO: The policy for eviction is up to you. */
	if (list_empty(&frame_table))
	{
		// printf("들어오지마\n");
		return NULL;
	}
	struct list_elem *frame_elem = list_begin(&frame_table);
	while (frame_elem != list_end(&frame_table))
	{
		struct frame *victim = list_entry(frame_elem, struct frame, frame_elem);
		if (victim->page->operations->type != VM_UNINIT)
		{
			list_pop_front(&frame_table);
			// printf("들오와야겠지..??\n");
			return victim;
		}
		frame_elem = frame_elem->next;
	}
	return NULL;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	if (victim == NULL)
	{
		// printf("들어오면 안돼!!\n");
		return NULL;
	}
	/* TODO: swap out the victim and return the evicted frame. */
	bool success = swap_out(victim->page);

	if (!success)
		PANIC("엘렐레");

	return victim;
	// if (success)
	// {
	// 	struct frame *copy_frame = (struct frame *)malloc(sizeof(struct frame));
	// 	memcpy(copy_frame, victim, sizeof(struct frame));
	// 	// pml4_clear_page(&victim->page->cur_thread->pml4, victim->page->va);
	// 	return copy_frame;
	// }
	// else
	// {
	// 	return NULL;
	// }
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* 물리 메모리의 userpool에서 새로운 물리 메모리 페이지를 가져온다. */
static struct frame *
vm_get_frame(void)
{
	/* TODO: Fill this function. */

	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);
	// printf("@@@@@%p\n",frame->kva);

	if (frame->kva == NULL)
	{
		// swap out이 발생 해야함
		struct frame *evict_frame = vm_evict_frame();
		memcpy(frame, evict_frame, sizeof(struct frame));
		frame->kva = pg_round_down(frame->kva);
	}

	frame->page = NULL;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	// 1. 하나 이상의 anon 페이지를 할당하여 스택 크기를 늘린다.
	// 2.  addr은 fault에서 유효한 주소가 된다.
	// 3. PGSIZE를 기준으로 내린다.
	// 4. 2^20 (1MB) 크기 제한을 조정
	intptr_t cur_rsp = thread_current()->rsp;

	// 함수 호출
	if (addr >= cur_rsp || addr == cur_rsp - 8)
	{
		// size_t total_date_size = cur_rsp - (int32_t)pg_addr;

		// printf("-------\n");
		// printf("(this is stack) pgaddr : %p\n", pg_addr);
		// printf("cur_rsp : %p\n", cur_rsp);
		// printf("total_date_size: %p\n", abs(total_date_size));
		// printf("pg_round_down(cur_rsp): %p\n",pg_round_down(cur_rsp));
		// printf("-------\n");

		vm_alloc_page(VM_ANON, pg_round_down(addr), true);
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	if (!is_user_vaddr(addr)) // 유저 address 영역인지 검사 (bool user는 유저모드이면 true, 커널모드이면 false) // modify
	{
		return false;
	}
	// page fault가 일어난 페이지 주소 pg_addr
	void *pg_addr = pg_round_down(addr); // 페이지 시작 주소부터

	// printf("cur_rsp: %p\n",cur_rsp);
	// printf("f->rsp : %p\n", f->rsp);

	// 스택 영역 존재 확인
	if (pg_addr >= MAX_STACK && pg_addr <= USER_STACK)
	{

		// 스택이 증가했는지
		if (user)
		{
			// printf("유저모드\n");
			thread_current()->rsp = f->rsp;
			if (thread_current()->rsp < f->rsp)
				return false;
		}

		// printf("addr : %p\n",addr );
		// printf("cur_rsp : %p\n",f->rsp);
		vm_stack_growth(addr);
	}
	if (not_present)
	{

		struct page *page = spt_find_page(spt, pg_addr);

		if (page == NULL)
			return false;

		// printf("current_spt_size: %d\n",spt->spt_hash.elem_cnt);
		return vm_do_claim_page(page);
	}

	// 스택 포인터를 저장하는 건 (유저 -> 커널) 전환될 때 즉, systemcall handler 또는  page_fault가 발생할 때,
	// page_fault로 전달된 if에서  rsp를 읽으면 유저 스택 포인터가 아닌 정의되지 않은 값을 얻을 수 있다.

	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
/* va에 할당된 페이지를 확보한다. */
bool vm_claim_page(void *va UNUSED)
{
	// struct page *page = (struct page *)malloc(sizeof(struct page));
	/* TODO: Fill this function */
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *p = spt_find_page(spt, va);
	if (p == NULL)
		return false;
	return vm_do_claim_page(p);
}

/* Claim the PAGE and set up the mmu. */
/* 페이지를 확보하고 mmu를 설정 */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	// printf("frame kva : %p\n", frame->kva);
	struct thread *cur = thread_current();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	bool succ = false;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (pml4_get_page(cur->pml4, page->va) != NULL)
	{
		return false;
	}
	succ = pml4_set_page(cur->pml4, page->va, frame->kva, page->writable);
	// printf("kva : %p\n",frame->kva);
	// printf("va : %p\n",page->va);
	succ = swap_in(page, frame->kva);
	list_push_back(&frame_table, &frame->frame_elem);
	// printf("do_kva : %p\n", frame->kva);
	if (!succ)
		vm_dealloc_page(page);
	return succ;
}
/* Initialize new supplemental page table */

/*보조 페이지 테이블을 초기화*/
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{

	hash_init(&(spt->spt_hash), page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
// 'src'에서 'dst'로 보조 페이지 테이블 복사
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	// struct hash_iterator i;
	// hash_first(&i, &src->spt_hash);
	// while (hash_next(&i))
	// {
	// 	struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
	// 	if(p != NULL)
	// 		hash_insert(&dst->spt_hash, &p->hash_elem);
	// }
	// if (hash_size(&dst->spt_hash) == hash_size(&src->spt_hash))
	// 	return true;
	// return false;

	src->spt_hash.aux = dst;
	// spt 테이블 순회
	hash_apply(&src->spt_hash, page_action_copy);
	if (hash_size(&dst->spt_hash) == hash_size(&src->spt_hash))
		return true;
	return false;
}

/* Free the resource hold by the supplemental page table */
// 보조 페이지 테이블이 차지하고 있는 자원을 해제
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// struct hash_iterator i;
	// hash_first(&i, &spt->spt_hash);
	// while (hash_next(&i))
	// {
	// 	struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
	// 	if(p != NULL)
	// 		vm_dealloc_page(p);
	// }
	// printf("@@@@@@%d\n",spt->spt_hash.elem_cnt);
	// if (spt->spt_hash.elem_cnt > 0)
	hash_clear(&spt->spt_hash, page_action_kill);
}
// hash_elem을 사용해 page를 찾기
uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED)
{
	const struct page *page = hash_entry(e, struct page, hash_elem);

	return hash_bytes(&page->va, sizeof page->va);
}
// 비교 함수
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}

void page_action_copy(struct hash_elem *e, void *aux UNUSED)
{
	// dst는 복사본 , e는 부모의 hash_elem
	struct supplemental_page_table *dst = aux;
	// page는 원본 페이지
	const struct page *src_page = hash_entry(e, struct page, hash_elem);

	// 초기화 되지않은 페이지 일 경우
	if (src_page->operations->type == VM_UNINIT)
	{
		// 페이지 할당 및 초기화 이후 spt table에 page 삽입
		vm_alloc_page_with_initializer(page_get_type(src_page), src_page->va, src_page->writable, src_page->uninit.init, src_page->uninit.aux);
	}
	// 페이지가 uninit이 아닐 경우
	else
	{
		// 페이지 할당 및 초기화 이후 spt table에 page 삽입
		vm_alloc_page(page_get_type(src_page), src_page->va, src_page->writable);
		struct page *dst_page = spt_find_page(dst, src_page->va);

		if (vm_do_claim_page(dst_page))
		{
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
		// vm_do_claim_page(src_page);
	}

	// 1. uninit일때와 uninit이 아닐경우 구분해서 분기문 만들기
	// 2. uninit 아닐때
	// vm_alloc_page(VM_ANON,NULL,true);
	// 3. uninit 일때
	// vm_alloc_page_with_initializer(page_get_type(src_page), src_page->va, src_page->writable, src_page->uninit.init, src_page->uninit.aux);

	// hash_insert(&dst->spt_hash, e);
	// spt_insert_page(&dst,p);
}

void page_action_kill(struct hash_elem *e, void *aux UNUSED)
{
	const struct page *p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}
