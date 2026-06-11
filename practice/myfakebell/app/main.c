#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEV_PATH "/dev/myfakebell0"
#define MYFB_IOC_MAGIC 42

struct rbdata {
    uint64_t timestamp_ns;
    uint32_t sample_value;
    uint32_t status;
};

struct statusmsg {
    uint32_t status;
    uint32_t threshold;
    uint32_t last_sample;
};

struct statmsg {
    uint32_t bufcnt;
    uint32_t dropcnt;
    uint32_t irqcnt;
};

#define FB_GET_STATUS     _IOR(MYFB_IOC_MAGIC, 0, struct statusmsg)
#define FB_SET_THRESHOLD  _IOW(MYFB_IOC_MAGIC, 1, uint32_t)
#define FB_CLEAR_ALARM    _IOW(MYFB_IOC_MAGIC, 2, uint32_t)
#define FB_GET_STATS      _IOR(MYFB_IOC_MAGIC, 3, struct statmsg)
#define FB_RESET_BUFFER   _IOW(MYFB_IOC_MAGIC, 4, uint32_t)

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s read\n"
            "  %s alarm <threshold>\n"
            "  %s stats\n",
            prog, prog, prog);
}

static int do_read_mode(void)
{
    int fd;
    struct pollfd pfd;

    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN | POLLRDNORM | POLLPRI;

    printf("[read] waiting on %s\n", DEV_PATH);

    while (1) {
        struct rbdata buf[8];
        ssize_t n;
        int ret;
        int i, cnt;

        pfd.revents = 0;
        ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            perror("poll");
            close(fd);
            return 1;
        }

        printf("[read] revents=0x%x\n", pfd.revents);

        if (pfd.revents & POLLPRI)
            printf("[read] alarm/high-priority event detected\n");

        if (pfd.revents & (POLLIN | POLLRDNORM)) {
            n = read(fd, buf, sizeof(buf));
            if (n < 0) {
                perror("read");
                close(fd);
                return 1;
            }
            if (n == 0) {
                printf("[read] EOF\n");
                close(fd);
                return 0;
            }

            cnt = (int)(n / sizeof(buf[0]));
            printf("[read] got %zd bytes, %d samples\n", n, cnt);

            for (i = 0; i < cnt; i++) {
                printf("  sample[%d]: ts=%llu ns value=%u status=0x%x\n",
                       i,
                       (unsigned long long)buf[i].timestamp_ns,
                       buf[i].sample_value,
                       buf[i].status);
            }
        }

        if (pfd.revents & POLLERR)
            printf("[read] POLLERR\n");
        if (pfd.revents & POLLHUP)
            printf("[read] POLLHUP\n");
    }
}

static int do_alarm_mode(uint32_t threshold)
{
    int fd;
    struct pollfd pfd;

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, FB_SET_THRESHOLD, &threshold) < 0) {
        perror("ioctl(FB_SET_THRESHOLD)");
        close(fd);
        return 1;
    }

    printf("[alarm] threshold set to %u\n", threshold);

    pfd.fd = fd;
    pfd.events = POLLPRI | POLLIN | POLLRDNORM;

    while (1) {
        int ret;
        struct statusmsg st;
        uint32_t dummy = 0;

        pfd.revents = 0;
        ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            perror("poll");
            close(fd);
            return 1;
        }

        printf("[alarm] revents=0x%x\n", pfd.revents);

        if (pfd.revents & (POLLPRI | POLLIN | POLLRDNORM)) {
            if (ioctl(fd, FB_GET_STATUS, &st) < 0) {
                perror("ioctl(FB_GET_STATUS)");
                close(fd);
                return 1;
            }

            printf("[alarm] status=0x%x threshold=%u last_sample=%u\n",
                   st.status, st.threshold, st.last_sample);

            if (ioctl(fd, FB_CLEAR_ALARM, &dummy) < 0) {
                perror("ioctl(FB_CLEAR_ALARM)");
                close(fd);
                return 1;
            }

            printf("[alarm] clear alarm requested\n");
        }

        if (pfd.revents & POLLERR)
            printf("[alarm] POLLERR\n");
        if (pfd.revents & POLLHUP)
            printf("[alarm] POLLHUP\n");
    }
}

static int do_stats_mode(void)
{
    int fd;

    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("[stats] reading stats from %s\n", DEV_PATH);

    while (1) {
        struct statmsg st;

        if (ioctl(fd, FB_GET_STATS, &st) < 0) {
            perror("ioctl(FB_GET_STATS)");
            close(fd);
            return 1;
        }

        printf("[stats] bufcnt=%u dropcnt=%u irqcnt=%u\n",
               st.bufcnt, st.dropcnt, st.irqcnt);

        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "read") == 0)
        return do_read_mode();

    if (strcmp(argv[1], "alarm") == 0) {
        uint32_t threshold = 50;
        if (argc >= 3)
            threshold = (uint32_t)strtoul(argv[2], NULL, 0);
        return do_alarm_mode(threshold);
    }

    if (strcmp(argv[1], "stats") == 0)
        return do_stats_mode();

    usage(argv[0]);
    return 1;
}
