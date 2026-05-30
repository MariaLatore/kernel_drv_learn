#include "fakesensor.h"
void init_tempstat(struct mysensor_status *stat) {
  stat->temp = 2;
  stat->humidity = 4;
  stat->alarm = 0;
  stat->threshold = 100;
  return;
}

int get_temp(struct mysensor_status *stat) { return stat->temp; }

int set_temp(struct mysensor_status *stat, int val) {
  int old = stat->temp;
  stat->temp = val;
  if (stat->temp > stat->threshold)
    stat->alarm = 1;
  return old;
}

int get_hum(struct mysensor_status *stat) { return stat->humidity; }

int set_hum(struct mysensor_status *stat, int val) {
  int old = stat->humidity;
  stat->humidity = val;
  return old;
}

int get_alm(struct mysensor_status *stat) { return stat->alarm; }

int set_alm(struct mysensor_status *stat, int val) {
  int old = stat->alarm;
  stat->alarm = val;
  return old;
}

int get_lim(struct mysensor_status *stat) { return stat->threshold; }

int set_lim(struct mysensor_status *stat, int val) {
  int old = stat->threshold;
  stat->threshold = val;
  return old;
}
