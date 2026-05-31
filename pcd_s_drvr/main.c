#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEV_MEM_SIZE 512
char device_buffer[DEV_MEM_SIZE];

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "%s :" fmt, __func__

/*Device number of pcd */
dev_t device_num;

	loff_t pcd_seek(struct file *filep, loff_t off, int  whence)
	{
		switch(whence)
		{
			case SEEK_SET:
			{
				if((off > DEV_MEM_SIZE) || (off < 0))
					return -EINVAL;
				
				filep->f_pos = off;
				break;
			}
			case SEEK_CUR:
			{
				loff_t temp = filep->f_pos + off;
				if(((temp  > DEV_MEM_SIZE) || (temp < 0)))
					return -EINVAL;
				filep->f_pos += off;
				break;
			}
			case SEEK_END:
			{
				loff_t temp = DEV_MEM_SIZE + off;
				if((temp > DEV_MEM_SIZE) || (temp < 0))
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
		pr_info("read requested for %zu bytes \n", size);
		pr_info("Current file position %lld\n" ,*f_pos);
		if((*f_pos + size) > DEV_MEM_SIZE)
		{
			size =  DEV_MEM_SIZE - *f_pos;
		}

		/* copy to user */
		if(copy_to_user(buff,&device_buffer[*f_pos], size)) 
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
		pr_info("write requested for %zu bytes \n", size);
		pr_info("Current file position %lld\n", *f_pos);
		if((*f_pos + size) > DEV_MEM_SIZE)
		{
			size =  DEV_MEM_SIZE - *f_pos;
		}

		/*Buffer full check*/
		if(!size) return -ENOMEM;

		/* copy from user */
		if(copy_from_user(&device_buffer[*f_pos], buff, size)) 
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
		pr_info("open was successful\n");
		return 0;
	}
	int pcd_release(struct inode * pinode, struct file *filep)
	{
		pr_info("close was successful\n");
		return 0;
	}

/*need a local structrue for cdev and fops for cdev_init(this is vfs registration */

struct cdev pcd_cdev;

struct file_operations pcd_fops = {

	.llseek = pcd_seek,
	.read = pcd_read,
	.write = pcd_write, 
	.open = pcd_open,
	.release = pcd_release
	
};

struct class *pcd_class;
struct device *pcd_device;

static int __init pcd_init(void)
{
	int ret;
	/* Dynamically allocate a device number */
	ret = alloc_chrdev_region(&device_num,0,1,"pcd");
	if(ret < 0)
		goto out;
	
	/*Register the device (cdev struct) with VFS -cdev_init() and cdev_add()*/
	cdev_init(&pcd_cdev, &pcd_fops);

	pcd_cdev.owner = THIS_MODULE;
    pcd_fops.owner = THIS_MODULE;	

	ret = cdev_add(&pcd_cdev, device_num,1);
	if(ret < 0)
		goto unreg_cdev;

	/*Create class - populatees in sys/class/ and device_create populates - sys/class/class_name/*/
	pcd_class = class_create(THIS_MODULE, "pcd_class");
	if(IS_ERR(pcd_class))
	{
		pr_err("Class creation failed\n");
		ret = PTR_ERR(pcd_class);
		goto cdev_del;
	}
	pcd_device = device_create(pcd_class, NULL, device_num, NULL, "pcd_device");
	if(IS_ERR(pcd_device))
	{
		pr_err("Device Crreation failed\n");
		ret = PTR_ERR(pcd_device);
		goto class_destroy;
	}

	pr_info("pcd_init success");
	return 0;

class_destroy:
	/*destory the class*/
	class_destroy(pcd_class);

cdev_del:
	/*Unregister cdev i.e. pcd from VFS*/
	cdev_del(&pcd_cdev);

unreg_cdev:
	/*Unregister the device number*/
	unregister_chrdev_region(device_num,1);
out:
	return ret;

}


static void __exit pcd_exit(void)
{
	/*Needs to be done in chronologically reverse order*/

	device_destroy(pcd_class, device_num);

	class_destroy(pcd_class);

	/*Unregister cdev i.e. pcd from VFS*/
	cdev_del(&pcd_cdev);

	/*Unregister the device number*/
	unregister_chrdev_region(device_num,1);
	
	pr_info("Module unloaded");
}

module_init(pcd_init);
module_exit(pcd_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sachin");
MODULE_DESCRIPTION("Simple pseudo char Driver");
		


