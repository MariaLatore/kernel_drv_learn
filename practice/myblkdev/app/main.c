#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define DEV_PATH "/dev/myblk0"

#define MYBLK_DATA_SIZE (16 * 4096)
#define MYBLK_CTRL_SIZE 4096
#define MYBLK_MAP_SIZE  (MYBLK_CTRL_SIZE + MYBLK_DATA_SIZE)

struct myblk_ring_ctrl {
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t overflow;
};

static void dump_ctrl(struct myblk_ring_ctrl *ctrl)
{
    printf("[ctrl] head=%u tail=%u size=%u overflow=%u\n",
           ctrl->head, ctrl->tail, ctrl->size, ctrl->overflow);
}

static void consume_ring(struct myblk_ring_ctrl *ctrl, char *data)
{
    while (ctrl->tail != ctrl->head) {
        uint32_t tail = ctrl->tail;
        uint32_t head = ctrl->head;

        if (tail < head) {
            size_t len = head - tail;
            fwrite(data + tail, 1, len, stdout);
            fflush(stdout);
            ctrl->tail = head;
        } else {
            size_t len = ctrl->size - tail;
            fwrite(data + tail, 1, len, stdout);
            fflush(stdout);
            ctrl->tail = 0;
        }
    }
}

int main(void)
{
    int fd;
    void *base;
    struct myblk_ring_ctrl *ctrl;
    char *data;
    struct pollfd pfd;
    int ret;

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    base = mmap(NULL, MYBLK_MAP_SIZE, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    ctrl = (struct myblk_ring_ctrl *)base;
    data = (char *)base + MYBLK_CTRL_SIZE;

    printf("mmap success: %s\n", DEV_PATH);
    dump_ctrl(ctrl);

    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        ret = poll(&pfd, 1, 5000);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (ret == 0) {
            printf("poll timeout\n");
            dump_ctrl(ctrl);
            continue;
        }

        if (pfd.revents & POLLIN) {
            printf("[event] POLLIN\n");
            dump_ctrl(ctrl);
            consume_ring(ctrl, data);
            printf("\n");
            dump_ctrl(ctrl);
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            printf("poll error revents=0x%x\n", pfd.revents);
            break;
        }
    }

    munmap(base, MYBLK_MAP_SIZE);
    close(fd);
    return 0;
}
