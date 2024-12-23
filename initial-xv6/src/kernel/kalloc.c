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

// Define the maximum number of physical pages
#define MAX_PHYS_PAGES (PHYSTOP / PGSIZE)

// Reference count array
int phys_page_ref[MAX_PHYS_PAGES];

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Initialize the memory allocator and reference counts
void
kinit(void)
{
  initlock(&kmem.lock, "kmem");
  // Initialize reference counts to zero
  memset(phys_page_ref, 0, sizeof(phys_page_ref));
  freerange(end, (void*)PHYSTOP);
}

// Increment the reference count of a physical page
void
incref(uint64 pa)
{
  // int index = (pa - (uint64)end) / PGSIZE;
  int index = pa / PGSIZE;
  if(index < 0 || index >= MAX_PHYS_PAGES)
    panic("incref: invalid physical address");

  acquire(&kmem.lock);
  phys_page_ref[index]++;
  release(&kmem.lock);
}


// Decrement the reference count of a physical page
void
decref(uint64 pa)
{
  // int index = (pa - (uint64)end) / PGSIZE;
  int index = pa / PGSIZE;
  if(index < 0 || index >= MAX_PHYS_PAGES)
    panic("decref: invalid physical address");

  acquire(&kmem.lock);
  if(phys_page_ref[index] > 0)
    phys_page_ref[index]--;

  // int refcnt = phys_page_ref[index];
  release(&kmem.lock);

  // if(refcnt == 0){
  //   kfree((void*)pa);
  // }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}


// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // int index = ((uint64)pa - PGROUNDDOWN((uint64)end)) / PGSIZE;
  int index = (uint64)pa / PGSIZE;
  if(index < 0 || index >= MAX_PHYS_PAGES)
    panic("kfree: invalid index");

  acquire(&kmem.lock);
  phys_page_ref[index]--;
  
  // Only free if reference count reaches 0
  if(phys_page_ref[index] <= 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
    phys_page_ref[index] = 0; // Ensure it's exactly 0
  }
  release(&kmem.lock);
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
  if(r)
  {
    kmem.freelist = r->next;
    memset((char*)r, 5, PGSIZE); // Fill with junk

    // int index = ((uint64)r - PGROUNDDOWN((uint64)end)) / PGSIZE;
    int index = (uint64)r / PGSIZE;
    if(index >= 0 || index < MAX_PHYS_PAGES)
      phys_page_ref[index] = 1;
  }
  release(&kmem.lock);

  return (void*)r;
}
