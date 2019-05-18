#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "ku_pir.h"

int ku_pir_open();
int ku_pir_close(int fd);
void ku_pir_read(int fd, struct ku_pir_data *data);
void ku_pir_flush(int fd);
int ku_pir_insertData(long unsigned int ts, char rf_flag);

int ku_pir_open()
{
	int dev = open("/dev/ku_pir_dev", O_RDWR);
	int fd = ioctl(dev, IOCTL_PIRQ_CREATE_NUM, 0);
	close(dev);

	return fd;
}

int ku_pir_close(int fd)
{
	int dev = open("/dev/ku_pir_dev", O_RDWR);
	int rtnValue = ioctl(dev, IOCTL_PIRQ_DELETE_NUM, &fd);
	close(dev);

	return rtnValue;
}

void ku_pir_read(int fd, struct ku_pir_data *data)
{
	int dev = open("/dev/ku_pir_dev", O_RDWR);
	
	struct fd_pir_data *read_data;
	read_data = (struct fd_pir_data *)malloc(sizeof(struct fd_pir_data));
	read_data->fd = fd;
	read_data->data = (struct ku_pir_data *)malloc(sizeof(struct ku_pir_data));

	ioctl(dev, IOCTL_PIR_READ_NUM, (unsigned long)read_data);
	close(dev);

	data->timestamp = read_data->data->timestamp;
	data->rf_flag = read_data->data->rf_flag;
}

void ku_pir_flush(int fd)
{
	int dev = open("/dev/ku_pir_dev", O_RDWR);
	ioctl(dev, IOCTL_PIRQ_RESET_NUM, &fd);
	close(dev);
}

int ku_pir_insertData(long unsigned int ts, char rf_flag)
{
	int dev = open("/dev/ku_pir_dev",O_RDWR);

	// Send data to kernel
	struct ku_pir_data *tmp_data;
	tmp_data = (struct ku_pir_data *)malloc(sizeof(struct ku_pir_data));
	tmp_data->timestamp = ts;
	tmp_data->rf_flag = rf_flag;

	int rtnValue;
	rtnValue = ioctl(dev, IOCTL_PIR_WRITE_NUM, (unsigned long)tmp_data);

	close(dev);
	return rtnValue;
}