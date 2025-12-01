// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUFMAP_HASH(dev, blockno) (((dev) ^ (blockno)) % NBUFMAP_BUCKET)

struct {
  struct spinlock lock;
  struct spinlock refhash_lock[NBUFMAP_BUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf refhash[NBUFMAP_BUCKET];
} bcache;

char* refhash_lock_names[NBUFMAP_BUCKET] = {
  "refhash_0",
  "refhash_1",
  "refhash_2",
  "refhash_3",
  "refhash_4",
  "refhash_5",
  "refhash_6",
  "refhash_7",
  "refhash_8",
  "refhash_9",
  "refhash_10",
  "refhash_11",
  "refhash_12",
};

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < NBUFMAP_BUCKET; i++) {
    initlock(&bcache.refhash_lock[i], refhash_lock_names[i]);
    bcache.refhash[i].next = 0;
  }

  // Initialize every buffer
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->refcnt = 0;
    b->lastuse_ticks = ticks;
    initsleeplock(&b->lock, "buffer");

    // Add all buffers to the first hash bucket's linked list
    b->next = bcache.refhash[0].next;
    bcache.refhash[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int idx = NBUFMAP_HASH(dev, blockno);

  acquire(&bcache.refhash_lock[idx]);

  // Is the block already cached?
  for(b = bcache.refhash[idx].next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.refhash_lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.refhash_lock[idx]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  acquire(&bcache.lock);

  for(b = bcache.refhash[idx].next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      acquire(&bcache.refhash_lock[idx]);
      b->refcnt++;
      release(&bcache.refhash_lock[idx]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf *best_prev = 0;
  int holding_idx = -1;
  for(int cnt = 0; cnt < NBUFMAP_BUCKET; cnt++) {
    acquire(&bcache.refhash_lock[cnt]);

    int found = 0;

    for(b = &bcache.refhash[cnt]; b->next; b = b->next) {
      if(b->next->refcnt == 0 && (!best_prev || b->next->lastuse_ticks < best_prev->next->lastuse_ticks)) {
        best_prev = b;
        found = 1;
      }
    }
    if(!found) {
      release(&bcache.refhash_lock[cnt]);
    } else {
      if(holding_idx != -1) {
        release(&bcache.refhash_lock[holding_idx]);
      }
      holding_idx = cnt;
    }
  }

  if(!best_prev) {
    panic("bget: no buffers");
  }

  b = best_prev->next;
  if(holding_idx != idx) {
    // Remove b from its current hash bucket list
    best_prev->next = b->next;
    release(&bcache.refhash_lock[holding_idx]);

    // Add b to the new hash bucket list
    acquire(&bcache.refhash_lock[idx]);
    b->next = bcache.refhash[idx].next;
    bcache.refhash[idx].next = b;
  }
  
  b->refcnt = 1;
  b->lastuse_ticks = ticks;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;

  release(&bcache.refhash_lock[idx]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int idx = NBUFMAP_HASH(b->dev, b->blockno);

  releasesleep(&b->lock);

  acquire(&bcache.refhash_lock[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastuse_ticks = ticks;
    // Remove from refhash list
  }
  
  release(&bcache.refhash_lock[idx]);
}

void
bpin(struct buf *b) {
  int idx = NBUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.refhash_lock[idx]);
  b->refcnt++;
  release(&bcache.refhash_lock[idx]);
}

void
bunpin(struct buf *b) {
  int idx = NBUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.refhash_lock[idx]);
  b->refcnt--;
  release(&bcache.refhash_lock[idx]);
}


