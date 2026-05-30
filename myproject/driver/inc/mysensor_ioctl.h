#ifndef _MYSENSOR_IOCTL_H_
#define _MYSENSOR_IOCTL_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint32_t __u32;
#endif

#define MYSENSOR_IOC_MAGIC 'M'

struct mysensor_status {
  __u32 temp;
  __u32 humidity;
  __u32 threshold;
  __u32 alarm;
};

/* query commands */
#define MYSENSOR_IOC_GET_STATUS                                                \
  _IOR(MYSENSOR_IOC_MAGIC, 0, struct mysensor_status)
#define MYSENSOR_IOC_GET_TEMP _IOR(MYSENSOR_IOC_MAGIC, 1, int)
#define MYSENSOR_IOC_GET_HUMIDITY _IOR(MYSENSOR_IOC_MAGIC, 2, int)
#define MYSENSOR_IOC_GET_THRESHOLD _IOR(MYSENSOR_IOC_MAGIC, 3, int)
#define MYSENSOR_IOC_GET_ALARM _IOR(MYSENSOR_IOC_MAGIC, 4, int)

/* set commands */
#define MYSENSOR_IOC_SET_TEMP _IOW(MYSENSOR_IOC_MAGIC, 5, int)
#define MYSENSOR_IOC_SET_HUMIDITY _IOW(MYSENSOR_IOC_MAGIC, 6, int)
#define MYSENSOR_IOC_SET_THRESHOLD _IOW(MYSENSOR_IOC_MAGIC, 7, int)

/* control commands */
#define MYSENSOR_IOC_CLEAR_ALARM _IO(MYSENSOR_IOC_MAGIC, 8)
#define MYSENSOR_IOC_TRIGGER_ALARM _IO(MYSENSOR_IOC_MAGIC, 9)

#endif /* _MYSENSOR_IOCTL_H_ */
