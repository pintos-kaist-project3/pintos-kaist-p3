/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

struct lock filesys_lock;
/* The initializer of file vm */
void vm_file_init(void)
{
	lock_init(&filesys_lock);
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
	// return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;

	void *save_addr = page->st_addr;
	// void *temp_addr = save_addr;
	if (!pml4_is_dirty(thread_current()->pml4, save_addr))
	{
		pml4_clear_page(thread_current()->pml4, save_addr);
	}
	else
	{
		struct binary_file *f = page->uninit.aux;
		struct frame *kpage = page->frame;
		lock_acquire(&filesys_lock);
		file_write_at(f->b_file, kpage->kva, PGSIZE, f->ofs);
		lock_release(&filesys_lock);
		// pml4_set_dirty(thread_current()->pml4, temp_addr, 0);
		// vm_dealloc_page(p);
		// destroy(p);
		// printf("bytes_written : %d\n",bytes_written);
	}
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
	int8_t *temp_addr = addr;
	// printf("file_length: %d\n",file_length);
	while (length > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */

		void *aux = NULL;
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct binary_file *b_file = (struct binary_file *)malloc(sizeof(struct binary_file));
		b_file->read_bytes = page_read_bytes;
		b_file->zero_bytes = page_zero_bytes;
		b_file->ofs = offset;
		b_file->b_file = file;

		// printf("read_bytes : %d\n", page_read_bytes);
		// printf("zero_bytes : %d\n", page_zero_bytes);
		// printf("ofs : %d\n", offset);
		// printf("------------------\n");

		// aux = b_file;
		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		if (!vm_alloc_page_with_initializer(VM_FILE, temp_addr,
											writable, lazy_load_segment, b_file))
			return NULL;

		/* Advance. */
		length -= page_read_bytes;
		// zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		struct supplemental_page_table *spt = &thread_current()->spt;
		struct page *p = spt_find_page(spt, temp_addr);
		p->st_addr = addr;
		temp_addr += PGSIZE;
	}

	// struct page *page = spt_find_page(&cur->spt, addr);
	// vm_claim_page(temp_addr);
	return addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *p= spt_find_page(spt,addr);
	if (p ==NULL){
		return;
	}
	// void *save_addr = p->st_addr;
	// void *temp_addr = save_addr;
	while (p!=NULL){
		destroy(p);
		addr += PGSIZE;
		p = spt_find_page(spt,addr);
	}
}
