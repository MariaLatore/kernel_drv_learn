
#ifndef _FAKESENSOR_H_
#define _FAKESENSOR_H_
#include "mysensor_ioctl.h"
void init_tempstat(struct mysensor_status *);
int get_temp(struct mysensor_status *);
int set_temp(struct mysensor_status *, int);
int get_hum(struct mysensor_status *);
int set_hum(struct mysensor_status *, int);
int get_alm(struct mysensor_status *);
int set_alm(struct mysensor_status *, int);
int get_lim(struct mysensor_status *);
int set_lim(struct mysensor_status *, int);

#endif
