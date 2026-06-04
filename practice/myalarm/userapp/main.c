#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>

#define DEV_PATH "/dev/myalarm0"

int main(void)
{
	int fd;
	int ret;
	struct pollfd pfd;
	char buf[64];

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	printf("opened %s\n", DEV_PATH);

	pfd.fd = fd;
	pfd.events = POLLIN;

	while (1) {
		printf("waiting for alarm ...\n");

		ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			perror("poll");
			break;
		}

		if (ret == 0) {
			printf("poll timeout\n");
			continue;
		}

		if (pfd.revents & POLLIN) {
			memset(buf, 0, sizeof(buf));

			ret = read(fd, buf, sizeof(buf) - 1);
			if (ret < 0) {
				perror("read");
				break;
			}

			buf[ret] = '\0';
			printf("alarm event: %s", buf);

			ret = write(fd, "0", 1);
			if (ret < 0) {
				perror("write clear");
				break;
			}

			printf("alarm cleared\n");
		}

		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			printf("poll error: revents=0x%x\n", pfd.revents);
			break;
		}
	}

	close(fd);
	return 0;
}
