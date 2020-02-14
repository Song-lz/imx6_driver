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
	unsigned char write_buff[1];

	if (argc != 3) {
		printf("Usage error!\n");
		return -1;
	}

	file_name = argv[1];
	//write_buff[0] = atoi(argv[2]);
	write_buff[0] = argv[2][0];
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		printf("file: %s open failure!\n", file_name);
		return -1;
	}
	if ((write_buff[0] == '0') || (write_buff[0] == '1'))
	{
		ret = write(fd, write_buff, 1);
		if (ret < 0) {
			printf("file: %s write data failure!\n", file_name);
			close(fd);
			return -1;
		}
	}
	else if(write_buff[0] == '2')
	{
		while(1)
		{
			write_buff[0] = '1';
			ret = write(fd, write_buff, 1);
			if (ret < 0) {
				printf("file: %s write data failure!\n", file_name);
				close(fd);
				return -1;
			}
			usleep(100000);
			write_buff[0] = '0';
			ret = write(fd, write_buff, 1);
			if (ret < 0) {
				printf("file: %s write data failure!\n", file_name);
				close(fd);
				return -1;
			}
			usleep(900000);
		}
	}
	else
	{
		printf("param error!\n");
	}
	ret = close(fd);
	if (ret < 0) {
		printf("file: %s close failure!\n", file_name);
		return -1;
	}
	return 0;
}
