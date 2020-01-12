#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

int main(int argc, char* argv[])
{
	int fd, ret;
	char* file_name;
	unsigned char read_buff[1];
	unsigned char count = 0;

	if (argc != 2) {
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
	while (1)
	{
		ret = read(fd, read_buff, 1);
	//	if (ret <= 0)
	//		printf("file: %s read data failure!\n", file_name);
	//	else
		if (ret > 0)
		{
			printf("file: %s read data ok!\n", file_name);
			if (count++ > 10)
			{
				break;
			}
		}
	}
	ret = close(fd);
	if (ret < 0) {
		printf("file: %s close failure!\n", file_name);
		return -1;
	}
	return 0;
}
