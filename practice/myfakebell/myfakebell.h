#ifndef _FAKEBELL_H_
#define _FAKEBELL_H_
#include <linux/types.h>
#define MYFB_IOC_MAGIC 42
struct statusmsg {
  u32 status;
  u32 threshold;
  u32 last_sample;
};

struct statmsg {
  u32 bufcnt;
  u32 dropcnt;
  u32 irqcnt;
};

#define FB_GET_STATUS _IOR(MYFB_IOC_MAGIC, 0, struct statusmsg)

#define FB_SET_THRESHOLD _IOW(MYFB_IOC_MAGIC, 1, u32)

#define FB_CLEAR_ALARM _IOW(MYFB_IOC_MAGIC, 2, u32)

#define FB_GET_STATS _IOR(MYFB_IOC_MAGIC, 3, struct statmsg)

#define FB_RESET_BUFFER _IOW(MYFB_IOC_MAGIC, 4, u32)

#endif
