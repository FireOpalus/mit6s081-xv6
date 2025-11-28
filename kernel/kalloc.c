// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Counter of per physical page's references
// Create lock to protect
// Create some defines to get the index from physical address
struct spinlock kcnt_lock;
#define PA2KCNT_IDX(p) (((p) - KERNBASE) / PGSIZE)
#define KCNT_MAX_IDX PA2KCNT_IDX(PHYSTOP)
int kcnt[KCNT_MAX_IDX] = {0};
#define PA2KCNT(p) kcnt[PA2KCNT_IDX((uint64)(p))]

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kcnt_lock, "kcnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kcnt_lock);
  if(--PA2KCNT(pa) <= 0) {
     // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&kcnt_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    PA2KCNT(r) = 1;
  }
  release(&kmem.lock);

  if(r) 
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// create a new page and copy data from pa
void*
copy_new_phypage(uint64 pa)
{
  acquire(&kcnt_lock);
  if(PA2KCNT(pa) <= 1) {
    release(&kcnt_lock);
    return (void*)pa;
  }

  char* newpg;
  if((newpg = kalloc()) == 0) {
    release(&kcnt_lock);
    return 0;
  }
  memmove((void*)newpg, (void*)pa, PGSIZE);
  
  PA2KCNT(pa)--;

  release(&kcnt_lock);
  return (void*)newpg;
}

// add reference count of a physical page
void
add_page_ref(uint64 pa)
{
  acquire(&kcnt_lock);
  PA2KCNT(pa)++;
  release(&kcnt_lock);
}