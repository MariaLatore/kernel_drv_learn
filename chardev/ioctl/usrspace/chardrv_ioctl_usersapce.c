/* userspace_ioctl.c - the process to use ioctl's to control the kernel module
 * Until now we could have used cat fro input and output. But now we need to ioctl's, which require writing our own process.
 */

/* device specifics, such as ioctl numbers and the major device file. */
#include "../charioctl.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

int ioctl_set_msg(int file_desc, char *message)
{
	int ret_val;

	ret_val = ioctl(file_desc, IOCTL_SET_MSG, message);

	if(ret_val < 0) {
		printf("ioctl_set_msg failed;%d\n", ret_val);
	}
	return ret_val;
}

int ioctl_get_msg(int file_desc)
{
	int ret_val;
	char message[100] = { 0 };

	/* Warning - this is dangerous because we don't tell the kernel hwo far it's allowed to write, so it might overflow the buffer. In a real production program, we would have used 2 ioctls - one to tell the kernel the buffer length and another to give it the buffer to fill. */
	ret_val = ioctl(file_desc, IOCTL_GET_MSG, message);

	if(ret_val < 0) {
		printf("ioctl_get_msg failed:%d\n", ret_val);
	}
	printf("get_msg_message:%s", message);
	return ret_val;
}

int ioctl_get_nth_byte(int file_desc)
{
	int i,c;
	printf("get_nth_byte message:");
	i = 0;
	do{
		c=ioctl(file_desc, IOCTL_GET_NTH_BYTE, i++);

		if(c<0){
			printf("\n ioctl_get_nth_byte failed at the %d'th byte:\n", i);
			return c;
		}

		putchar(c);
	}while(c!=0);

	return 0;
}


/* Main - Call the ioctl functions */
int main(void)
{
	int file_desc, ret_val;
	char *msg = "Message passed by ioctl\n";

	file_desc = open(DEVICE_PATH, O_RDWR);
	if(file_desc < 0){
		printf("Can't open device file:%s. error:%d\n", DEVICE_PATH, file_desc);
		exit(EXIT_FAILURE);
	}

	ret_val = ioctl_set_msg(file_desc, msg);
	if(ret_val)
		goto error;
	ret_val = ioctl_get_nth_byte(file_desc);
	if(ret_val)
		goto error;
	ret_val = ioctl_get_msg(file_desc);
	if(ret_val)
		goto error;

	close(file_desc);
	return 0;
error:
	close(file_desc);
	exit(EXIT_FAILURE);
}
