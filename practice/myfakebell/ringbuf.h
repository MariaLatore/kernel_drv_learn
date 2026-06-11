#ifndef _RINGBUF_H_
#define _RINGBUF_H_
#define RINGBUF_SIZE 64
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>

struct rbdata {
  u64 timestamp_ns;
  u32 sample_value;
  u32 status;
};

struct ringbuf {
  struct rbdata data[RINGBUF_SIZE];
  int head;
  spinlock_t lk;
  int tail;
  int size;
  int dropcount;
};

static inline void ringbuf_init(struct ringbuf *r) {
  memset(r, 0, sizeof(*r));
  spin_lock_init(&r->lk);
}

static inline void ringbuf_push(struct ringbuf *r, struct rbdata *v) {
  unsigned long flags;
  spin_lock_irqsave(&r->lk, flags);
  if (r->size == 0) {
    memcpy(&r->data[r->head], v, sizeof(*v));
    r->head = (r->head + 1) % RINGBUF_SIZE;
    r->size++;
    spin_unlock_irqrestore(&r->lk, flags);
    return;
  }
  if (r->size == RINGBUF_SIZE) {
    r->tail = (r->tail + 1) % RINGBUF_SIZE;
    r->dropcount++;
  } else {
    r->size++;
  }
  memcpy(&r->data[r->head], v, sizeof(*v));
  r->head = (r->head + 1) % RINGBUF_SIZE;
  spin_unlock_irqrestore(&r->lk, flags);
  return;
}

static inline int ringbuf_pop(struct ringbuf *r, struct rbdata *v) {
  unsigned long flags;
  spin_lock_irqsave(&r->lk, flags);
  if (r->size == 0) {
    spin_unlock_irqrestore(&r->lk, flags);
    return -1;
  }
  memcpy(v, &r->data[r->tail], sizeof(*v));
  r->tail = (r->tail + 1) % RINGBUF_SIZE;
  r->size--;
  spin_unlock_irqrestore(&r->lk, flags);
  return 0;
}

static inline int ringbuf_getsize(struct ringbuf *r) {
  int size;
  unsigned long flags;
  spin_lock_irqsave(&r->lk, flags);
  size = r->size;
  spin_unlock_irqrestore(&r->lk, flags);
  return size;
}

static inline int ringbuf_getlost(struct ringbuf *r) {
  int drop;
  unsigned long flags;
  spin_lock_irqsave(&r->lk, flags);
  drop = r->dropcount;
  spin_unlock_irqrestore(&r->lk, flags);
  return drop;
}

static inline void ringbuf_resetlost(struct ringbuf *r) {
  unsigned long flags;
  spin_lock_irqsave(&r->lk, flags);
  r->dropcount = 0;
  spin_unlock_irqrestore(&r->lk, flags);
}

#endif
