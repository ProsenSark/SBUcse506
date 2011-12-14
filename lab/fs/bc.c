#include <inc/pincl.h>

#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (vpd[PDX(va)] & PTE_P) && (vpt[VPN(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (vpt[VPN(va)] & PTE_D) != 0;
}

// Fault any disk block that is read or written in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;
	void *dst_va;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Allocate a page in the disk map region and read the
	// contents of the block from the disk into that page.
	//
	// LAB 5: Your code here
	dst_va = diskaddr(blockno);
	r = sys_page_alloc(0, dst_va, PTE_W | PTE_U | PTE_P);
	if (r < 0) {
		panic("bc_pgfault: sys_page_alloc FAILED: %e", r);
	}
	r = ide_read(blockno * BLKSECTS, dst_va, BLKSECTS);
	if (r < 0) {
		panic("bc_pgfault: ide_read FAILED: %e", r);
	}

	// Sanity check the block number. (exercise for the reader:
	// why do we do this *after* reading the block in?)
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Check that the block we read was allocated.
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_USER constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;
	void *src_va;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	//panic("flush_block not implemented");
	if (!va_is_mapped(addr)) {
		return;
	}
	if (!va_is_dirty(addr)) {
		return;
	}

	//src_va = ROUNDDOWN(addr, BLKSIZE);
	src_va = diskaddr(blockno);
	r = ide_write(blockno * BLKSECTS, src_va, BLKSECTS);
	if (r < 0) {
		panic("flush_block: ide_write FAILED: %e", r);
	}
	// Clear dirty bit
	r = sys_page_map(0, src_va, 0, src_va, PTE_USER);
	if (r < 0) {
		panic("flush_block: sys_page_map FAILED: %e", r);
	}
}

void
flush_sector(void *addr)
{
	uint32_t secno = ((uint32_t)addr - DISKMAP) / SECTSIZE;
	int r;
	void *src_va;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE)) {
		panic("flush_sector of bad va %08x", addr);
	}

	if (!va_is_mapped(addr)) {
		return;
	}
	if (!va_is_dirty(addr)) {
		return;
	}

	src_va = (void *) (DISKMAP + secno * SECTSIZE);
	/*
	 * NOTE: The following is a single sector write to IDE disk, which is
	 * presumed to be atomic enough such that even if power fails while the
	 * write is in-progress, the disk hardware can gather enough power to
	 * write out the sector fully, probably from capacitors, etc.
	 */
	r = ide_write(secno, src_va, 1);
	if (r < 0) {
		panic("flush_block: ide_write FAILED: %e", r);
	}
#if 0
	// Clear dirty bit
	r = sys_page_map(0, src_va, 0, src_va, PTE_USER);
	if (r < 0) {
		panic("flush_block: sys_page_map FAILED: %e", r);
	}
#endif
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it 
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	set_pgfault_handler(bc_pgfault);
	check_bc();
}

