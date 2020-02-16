#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

int main(int argc, char* argv[])
{
	int fd, ret, cnt = 0;
	char* file_name;
	unsigned char write_buff[1];

	if (argc != 3) {
		printf("Usage error!\n");
		return -1;
	}

	file_name = argv[1];
//	write_buff[0] = atoi(argv[2]);
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		printf("file: %s open failure!\n", file_name);
		return -1;
	}
	ret = write(fd, argv[2], 1);
	if (ret < 0) {
		printf("file: %s write data failure!\n", file_name);
		close(fd);
		return -1;
	}
	while(1) {
		cnt++;
		sleep(1);
		printf("sleep %d seconds!\n", cnt);
		if (cnt >= 10)
			break;
	}
	ret = close(fd);
	if (ret < 0) {
		printf("file: %s close failure!\n", file_name);
		return -1;
	}
	return 0;
}
