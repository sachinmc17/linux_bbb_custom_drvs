#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 512
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 1024
#define TOTAL_DEVICES 		4

#define RDONLY 0x01
#define WRONLY 0X10
#define RDWR   0x11

char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];

/*index for devicex */
static int idx = 0;
#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "%s :" fmt, __func__

/*Ddevice private data structure*/

struct pcdev_private_data {
	char *buffer;
	unsigned int size;
	const char *serial_num;
	int perm;
	struct cdev cdev;
};

/*Driver private data structure*/
struct pcdrv_private_data {

	dev_t device_num;
	struct class *pcd_class;
	struct device *pcd_device;
	int total_devices;
	struct pcdev_private_data pcdev_data[TOTAL_DEVICES];
};

struct pcdrv_private_data pcdrv_data = {
	.total_devices = TOTAL_DEVICES,
	.pcdev_data = {
		[0] = {
			.buffer = device_buffer_pcdev1,
			.size = MEM_SIZE_MAX_PCDEV1,
			.serial_num = "PCDEV1",
			.perm = RDONLY /*RDONLY*/
		},
		[1] = {
			.buffer = device_buffer_pcdev2,
			.size = MEM_SIZE_MAX_PCDEV2,
			.serial_num = "PCDEV2",
			.perm = WRONLY /*WRONLY*/
		},
		[2] = {
			.buffer = device_buffer_pcdev3,
			.size = MEM_SIZE_MAX_PCDEV3,
			.serial_num = "PCDEV3",
			.perm = RDWR /*RDWR*/
		},
		[3] = {
			.buffer = device_buffer_pcdev4,
			.size = MEM_SIZE_MAX_PCDEV4,
			.serial_num = "PCDEV4",
			.perm = RDWR /*RDWR*/
		}
	}
};

/*Function to check permission of a specific char dev file*/
int check_permission(int dev_perm, int acc_mode)
{
	if(dev_perm == RDWR)
		return 0;

	if((dev_perm == RDONLY) && ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)))
		return 0;

	if((dev_perm == WRONLY) && ((acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)))
		return 0;

	return -EPERM; 
}

	loff_t pcd_seek(struct file *filep, loff_t off, int  whence)
	{

		struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filep->private_data;
		size_t max_size = pcdev_data->size;

		switch(whence)
		{
			case SEEK_SET:
			{
				if((off > max_size) || (off < 0))
					return -EINVAL;
				
				filep->f_pos = off;
				break;
			}
			case SEEK_CUR:
			{
				loff_t temp = filep->f_pos + off;
				if(((temp  > max_size) || (temp < 0)))
					return -EINVAL;
				filep->f_pos += off;
				break;
			}
			case SEEK_END:
			{
				loff_t temp = max_size + off;
				if((temp > max_size) || (temp < 0))
					return -EINVAL;
				filep->f_pos = temp;
				break;
			}
			default:
			{
				return -EINVAL;
			}
		}
		pr_info("New value of file pointer is %lld\n", filep->f_pos);

		return filep->f_pos;
	}
	ssize_t pcd_read(struct file *filep, char __user *buff, size_t size, loff_t *f_pos)
	{
		struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filep->private_data;

		size_t max_size = pcdev_data->size;

		pr_info("read requested for %zu bytes \n", size);
		pr_info("Current file position %lld\n" ,*f_pos);

		/*Adjust the size*/
		if((*f_pos + size) > max_size)
		{
			size =  max_size - *f_pos;
		}

		/* copy to user */
		if(copy_to_user(buff,pcdev_data->buffer + *f_pos, size)) 
			return -EFAULT;

		/*Update current line position*/

		*f_pos += size;
		pr_info("actual bytes read =  %zu bytes \n", size);
		pr_info("Updated value of the file position %lld\n", *f_pos);
		/*Return number of bytes successfuly read*/
		return size;
	}
	ssize_t pcd_write(struct file *filep, const char __user *buff, size_t size, loff_t *f_pos)
	{

		struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filep->private_data;

		size_t max_size = pcdev_data->size;


		pr_info("write requested for %zu bytes \n", size);
		pr_info("Current file position %lld\n", *f_pos);
		if((*f_pos + size) > max_size)
		{
			size =  max_size - *f_pos;
		}

		/*Buffer full check*/
		if(!size) return -ENOMEM;

		/* copy from user */
		if(copy_from_user(pcdev_data->buffer + *f_pos, buff, size)) 
			return -EFAULT;

		/*Update current line position*/

		*f_pos += size;
		pr_info("actual bytes written =  %zu bytes \n", size);
		pr_info("Updated value of the file position %lld\n", *f_pos);

		/*Return number of bytes successfuly written*/
		return size;
	}
	int pcd_open(struct inode * pinode, struct file *filep)
	{	
		int minor_num;
		struct pcdev_private_data *pcdev_data;


		minor_num = MINOR(pinode->i_rdev);
		pr_info("minor number opened %d\n", minor_num);

		/*fetch device private data structure tthrough the struct cdev via container_of*/

		pcdev_data = container_of(pinode->i_cdev, struct pcdev_private_data, cdev);

		/*Store our info in private_data for usage in operations like read/write/lseek etc.*/
		filep->private_data = pcdev_data;

		/*check permission */

		pr_info("open was successful\n");
		return 0;
	}
	int pcd_release(struct inode * pinode, struct file *filep)
	{
		pr_info("close was successful\n");
		return 0;
	}

struct file_operations pcd_fops = {

	.llseek = pcd_seek,
	.read = pcd_read,
	.write = pcd_write, 
	.open = pcd_open,
	.release = pcd_release
	
};

static int __init pcd_init(void)
{
	int ret;
	/* Dynamically allocate a device number */
	ret = alloc_chrdev_region(&pcdrv_data.device_num,0,TOTAL_DEVICES,"pcd");
	if(ret < 0)
		goto out;

	/*Create class - populates in sys/class/ and device_create populates - sys/class/class_name/*/
	pcdrv_data.pcd_class = class_create(THIS_MODULE, "pcd_class");
	if(IS_ERR(pcdrv_data.pcd_class))
	{
		pr_err("Class creation failed\n");
		ret = PTR_ERR(pcdrv_data.pcd_class);
		goto unreg_cdev;
	}

	pcd_fops.owner = THIS_MODULE;
	for(idx = 0; idx < TOTAL_DEVICES; idx++)
	{
		/*Register the device (cdev struct) with VFS -cdev_init() and cdev_add() for each device*/
		cdev_init(&pcdrv_data.pcdev_data[idx].cdev, &pcd_fops);
		pcdrv_data.pcdev_data[idx].cdev.owner = THIS_MODULE;

		ret = cdev_add(&pcdrv_data.pcdev_data[idx].cdev, pcdrv_data.device_num+idx,1);
		if(ret < 0)
			goto cdev_del;

		pcdrv_data.pcd_device = device_create(pcdrv_data.pcd_class, NULL, pcdrv_data.device_num+idx, NULL, "pcd_device_%d", idx+1);
		if(IS_ERR(pcdrv_data.pcd_device))
		{
			pr_err("Device Crreation failed\n");
			ret = PTR_ERR(pcdrv_data.pcd_device);
			goto class_destroy;
		}

	}

	pr_info("pcd_init success");
	return 0;

class_destroy:
cdev_del:
	for(; idx >= 0; idx--)
	{
		/*Destroy all the devices from idx failure*/
		device_destroy(pcdrv_data.pcd_class, pcdrv_data.device_num + idx);
		/*Unregister cdev i.e. pcd from VFS*/
		cdev_del(&pcdrv_data.pcdev_data[idx].cdev);
	}

	/*destory the class*/
	class_destroy(pcdrv_data.pcd_class);


unreg_cdev:
	/*Unregister the device number*/
	unregister_chrdev_region(pcdrv_data.device_num, TOTAL_DEVICES);
out:
	return ret;

}


static void __exit pcd_exit(void)
{
	int i;
	for(i = 0; i < TOTAL_DEVICES; i++)
	{
		/*Destroy all the devices from idx failure*/
		device_destroy(pcdrv_data.pcd_class, pcdrv_data.device_num + i);
		/*Unregister cdev i.e. pcd from VFS*/
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
	}

	/*destory the class*/
	class_destroy(pcdrv_data.pcd_class);

	/*Unregister the device number*/
	unregister_chrdev_region(pcdrv_data.device_num, TOTAL_DEVICES);
	
	pr_info("Module unloaded");
}

module_init(pcd_init);
module_exit(pcd_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sachin");
MODULE_DESCRIPTION("Psuedo char Driver for multiple devices");
		


