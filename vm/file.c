/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"

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

/* The initializer of file vm */
void vm_file_init(void)
{
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
	return true;
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
		temp_addr += PGSIZE;
	}

	// struct page *page = spt_find_page(&cur->spt, addr);
	// vm_claim_page(temp_addr);
	return addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
}
