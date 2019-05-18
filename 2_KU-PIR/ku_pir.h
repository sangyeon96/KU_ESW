#define KUPIR_MAX_MSG 10
#define KUPIR_SENSOR 17
#define DEV_NAME "ku_pir_dev"

#define IOCTL_START 0x80
#define IOCTL_PIRQ_CREATE IOCTL_START+1
#define IOCTL_PIRQ_DELETE IOCTL_START+2
#define IOCTL_PIRQ_RESET IOCTL_START+3
#define IOCTL_PIR_READ IOCTL_START+4
#define IOCTL_PIR_WRITE IOCTL_START+5

#define IOCTL_MAGICNUM 'z'
#define IOCTL_PIRQ_CREATE_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_PIRQ_CREATE, unsigned long *)
#define IOCTL_PIRQ_DELETE_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_PIRQ_DELETE, unsigned long *)
#define IOCTL_PIRQ_RESET_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_PIRQ_RESET, unsigned long *)
#define IOCTL_PIR_READ_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_PIR_READ, unsigned long *)
#define IOCTL_PIR_WRITE_NUM _IOWR(IOCTL_MAGICNUM, IOCTL_PIR_WRITE, unsigned long *)

struct ku_pir_data
{
	long unsigned int timestamp;
	char rf_flag; //RISING '0', FALLING '1'
};

struct fd_pir_data //pir data with fd
{
	int fd;
	struct ku_pir_data *data;
};