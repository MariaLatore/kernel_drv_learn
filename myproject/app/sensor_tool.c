#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "mysensor_ioctl.h"

static void usage(const char *prog) {
  printf("Usage:\n");
  printf("  %s <dev> get_status\n", prog);
  printf("  %s <dev> get_temp\n", prog);
  printf("  %s <dev> get_humidity\n", prog);
  printf("  %s <dev> get_threshold\n", prog);
  printf("  %s <dev> get_alarm\n", prog);
  printf("  %s <dev> set_temp <value>\n", prog);
  printf("  %s <dev> set_humidity <value>\n", prog);
  printf("  %s <dev> set_threshold <value>\n", prog);
  printf("  %s <dev> clear_alarm\n", prog);
  printf("  %s <dev> trigger_alarm\n", prog);
  printf("  %s <dev> wait_alarm\n", prog);
  printf("  %s <dev> wait_alarm_timeout <ms>\n", prog);
  printf("\nExample:\n");
  printf("  %s /dev/mychardev0 get_status\n", prog);
  printf("  %s /dev/mychardev0 set_temp 88\n", prog);
  printf("  %s /dev/mychardev0 wait_alarm\n", prog);
  printf("  %s /dev/mychardev0 wait_alarm_timeout 5000\n", prog);
}

static int do_get_int(int fd, unsigned long cmd, const char *name) {
  int val = 0;

  if (ioctl(fd, cmd, &val) < 0) {
    perror("ioctl");
    return -1;
  }

  printf("%s: %d\n", name, val);
  return 0;
}

static int do_set_int(int fd, unsigned long cmd, const char *name,
                      const char *arg) {
  int val = atoi(arg);

  if (ioctl(fd, cmd, &val) < 0) {
    perror("ioctl");
    return -1;
  }

  printf("%s set to %d\n", name, val);
  return 0;
}

static int do_wait_alarm(int fd, int timeout_ms) {
  struct pollfd pfd;
  int ret;

  pfd.fd = fd;
  pfd.events = POLLIN | POLLRDNORM;
  pfd.revents = 0;

  if (timeout_ms < 0)
    printf("waiting for alarm indefinitely...\n");
  else
    printf("waiting for alarm, timeout=%d ms...\n", timeout_ms);

  ret = poll(&pfd, 1, timeout_ms);
  if (ret < 0) {
    perror("poll");
    return -1;
  }

  if (ret == 0) {
    printf("poll timeout\n");
    return 0;
  }

  printf("poll returned: %d\n", ret);
  printf("revents: 0x%x", pfd.revents);

  if (pfd.revents & POLLIN)
    printf(" POLLIN");
  if (pfd.revents & POLLRDNORM)
    printf(" POLLRDNORM");
  if (pfd.revents & POLLERR)
    printf(" POLLERR");
  if (pfd.revents & POLLHUP)
    printf(" POLLHUP");
  if (pfd.revents & POLLNVAL)
    printf(" POLLNVAL");
  printf("\n");

  if (pfd.revents & (POLLIN | POLLRDNORM))
    printf("alarm event detected\n");

  return 0;
}

int main(int argc, char *argv[]) {
  int fd;
  const char *dev;

  if (argc < 3) {
    usage(argv[0]);
    return 1;
  }

  dev = argv[1];

  fd = open(dev, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  if (strcmp(argv[2], "get_status") == 0) {
    struct mysensor_status st;

    memset(&st, 0, sizeof(st));
    if (ioctl(fd, MYSENSOR_IOC_GET_STATUS, &st) < 0) {
      perror("ioctl");
      close(fd);
      return 1;
    }

    printf("temp=%u humidity=%u threshold=%u alarm=%u\n", st.temp, st.humidity,
           st.threshold, st.alarm);
  } else if (strcmp(argv[2], "get_temp") == 0) {
    if (do_get_int(fd, MYSENSOR_IOC_GET_TEMP, "temp") < 0)
      goto err;
  } else if (strcmp(argv[2], "get_humidity") == 0) {
    if (do_get_int(fd, MYSENSOR_IOC_GET_HUMIDITY, "humidity") < 0)
      goto err;
  } else if (strcmp(argv[2], "get_threshold") == 0) {
    if (do_get_int(fd, MYSENSOR_IOC_GET_THRESHOLD, "threshold") < 0)
      goto err;
  } else if (strcmp(argv[2], "get_alarm") == 0) {
    if (do_get_int(fd, MYSENSOR_IOC_GET_ALARM, "alarm") < 0)
      goto err;
  } else if (strcmp(argv[2], "set_temp") == 0) {
    if (argc < 4) {
      fprintf(stderr, "missing value for set_temp\n");
      goto usage_err;
    }
    if (do_set_int(fd, MYSENSOR_IOC_SET_TEMP, "temp", argv[3]) < 0)
      goto err;
  } else if (strcmp(argv[2], "set_humidity") == 0) {
    if (argc < 4) {
      fprintf(stderr, "missing value for set_humidity\n");
      goto usage_err;
    }
    if (do_set_int(fd, MYSENSOR_IOC_SET_HUMIDITY, "humidity", argv[3]) < 0)
      goto err;
  } else if (strcmp(argv[2], "set_threshold") == 0) {
    if (argc < 4) {
      fprintf(stderr, "missing value for set_threshold\n");
      goto usage_err;
    }
    if (do_set_int(fd, MYSENSOR_IOC_SET_THRESHOLD, "threshold", argv[3]) < 0)
      goto err;
  } else if (strcmp(argv[2], "clear_alarm") == 0) {
    if (ioctl(fd, MYSENSOR_IOC_CLEAR_ALARM) < 0) {
      perror("ioctl");
      goto err;
    }
    printf("alarm cleared\n");
  } else if (strcmp(argv[2], "trigger_alarm") == 0) {
    if (ioctl(fd, MYSENSOR_IOC_TRIGGER_ALARM) < 0) {
      perror("ioctl");
      goto err;
    }
    printf("alarm triggered\n");
  } else if (strcmp(argv[2], "wait_alarm") == 0) {
    if (do_wait_alarm(fd, -1) < 0)
      goto err;
  } else if (strcmp(argv[2], "wait_alarm_timeout") == 0) {
    int timeout_ms;

    if (argc < 4) {
      fprintf(stderr, "missing timeout(ms) for wait_alarm_timeout\n");
      goto usage_err;
    }

    timeout_ms = atoi(argv[3]);
    if (timeout_ms < 0) {
      fprintf(stderr, "timeout must be >= 0\n");
      goto err;
    }

    if (do_wait_alarm(fd, timeout_ms) < 0)
      goto err;
  } else {
    fprintf(stderr, "unknown command: %s\n", argv[2]);
    goto usage_err;
  }

  close(fd);
  return 0;

usage_err:
  usage(argv[0]);
err:
  close(fd);
  return 1;
}
