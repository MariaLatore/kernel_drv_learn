#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEV_PATH "/dev/mysensor0"
#define BUF_SIZE 128

static void test_open_close(void)
{
    int fd;

    printf("========== test open/release ==========\n");
    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    printf("open(%s) success, fd=%d\n", DEV_PATH, fd);

    if (close(fd) < 0) {
        perror("close");
        return;
    }

    printf("close() success\n\n");
}

static void test_nonblocking_read(void)
{
    int fd;
    char buf[BUF_SIZE];
    ssize_t n;

    printf("========== test nonblocking read ==========\n");

    fd = open(DEV_PATH, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return;
    }

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("nonblocking read: no data available, got EAGAIN as expected\n");
        } else {
            perror("read");
        }
    } else {
        buf[n] = '\0';
        printf("nonblocking read returned %zd bytes: %s", n, buf);
    }

    close(fd);
    printf("\n");
}

static void test_poll_and_read(void)
{
    int fd;
    struct pollfd pfd;
    char buf[BUF_SIZE];
    ssize_t n;
    int ret;

    printf("========== test poll ==========\n");

    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    printf("waiting up to 5000 ms for data ...\n");
    ret = poll(&pfd, 1, 5000);
    if (ret < 0) {
        perror("poll");
        close(fd);
        return;
    }

    if (ret == 0) {
        printf("poll timeout, no data arrived within 5 seconds\n");
        close(fd);
        printf("\n");
        return;
    }

    printf("poll returned %d, revents=0x%x\n", ret, pfd.revents);

    if (pfd.revents & POLLIN) {
        memset(buf, 0, sizeof(buf));
        n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            perror("read");
            close(fd);
            return;
        }

        buf[n] = '\0';
        printf("read after poll returned %zd bytes: %s", n, buf);
    }

    if (pfd.revents & POLLERR)
        printf("POLLERR set\n");
    if (pfd.revents & POLLHUP)
        printf("POLLHUP set\n");
    if (pfd.revents & POLLNVAL)
        printf("POLLNVAL set\n");

    close(fd);
    printf("\n");
}

static void test_blocking_read(void)
{
    int fd;
    char buf[BUF_SIZE];
    ssize_t n;

    printf("========== test blocking read ==========\n");

    fd = open(DEV_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    printf("calling blocking read(), waiting for sensor data ...\n");

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        close(fd);
        return;
    }

    buf[n] = '\0';
    printf("blocking read returned %zd bytes: %s", n, buf);

    close(fd);
    printf("\n");
}

int main(void)
{
    printf("Testing driver node: %s\n\n", DEV_PATH);

    test_open_close();
    test_nonblocking_read();
    test_poll_and_read();
    test_blocking_read();

    printf("All available interfaces tested.\n");
    return 0;
}
