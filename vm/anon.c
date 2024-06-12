/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "kernel/list.h"
#include "kernel/bitmap.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};
struct bitmap *swap_table;
int bitmap_idx;
/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	swap_table = bitmap_create(disk_size(swap_disk));
	bitmap_idx = 0;
}

/* Initialize the file mapping */
// 페이지 내부 구조체를 anon으로 갱신
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	list_push_back(&frame_table, &page->frame->frame_elem);
	struct frame *f = list_entry(list_begin(&frame_table), struct frame, frame_elem);
	// printf("type: %d\n",f->page->operations->type);
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{

	printf("in_kva : %p\n", kva);
	struct anon_page *anon_page = &page->anon;
	if (kva == NULL || page == NULL)
	{
		// printf("0\n");

		return false;
	}
	// printf("1\n");
	int idx = page->bitmap_idx;
	printf("swap in, page->bitmap_idx : %d\n", page->bitmap_idx);
	int j = 0;

	for (int i = 0; i < PGSIZE; i += DISK_SECTOR_SIZE)
	{
		bitmap_set(swap_table, idx, 0);
		idx++;
		// int capacity = disk_size(swap_disk);
		// printf("capacity : %d\n", capacity);
		// printf("4\n");

		disk_read(swap_disk, ((page->bitmap_idx) + j), kva);
		j++;
		kva += DISK_SECTOR_SIZE;
	}
	// printf("3\n");

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_pxage *anon_page = &page->anon;

	int sector_num = 0;
	page->bitmap_idx = bitmap_idx;

	printf("page->bitmap_idx : %d\n", page->bitmap_idx);
	void *kva = page->frame->kva;

	printf("swap out start kva : %p\n", kva);
	int j = 0;
	for (int i = 0; i < PGSIZE; i += DISK_SECTOR_SIZE)
	{
		bitmap_set(swap_table, bitmap_idx, 1);
		bitmap_idx++;
		// int capacity = disk_size(swap_disk);
		// printf("capacity : %d\n", capacity);
		disk_write(swap_disk, ((page->bitmap_idx ) + j), kva);
		j++;
		kva += DISK_SECTOR_SIZE;
	}
	printf("out_kva : %p\n", kva);

	// pml4_clear_page(&thread_current()->pml4, page->va);

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
}
