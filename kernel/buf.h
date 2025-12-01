struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint lastuse_ticks;    // For LRU caching.
  struct buf *next; // Hash bucket linked list
  uchar data[BSIZE];
};

