#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include "process_ioctl.h"

#define MSG_LEN 		64

#define MAX_USERS 		20

#define FIFO_SIZE 		128

typedef struct pdata {
	int pid;
	int free;
	struct kfifo_rec_ptr_1 fifo;
	spinlock_t slock;
} pdata;

pdata p[MAX_USERS];

static int N = 0;




static long int my_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	//file, command, flag
	//checking if command is to connect to chat
	int i, ret = -1, idx = -1;
	//first check cmd(command) connect user hai kya?
	if(cmd != CONNECT_USER)
	{
		return -1; // get out from it
	}
	//checking availabilty of process, if free allocated to current process
	for(i = 0; i < N; i++)

	// N is how process connected
	{
		if(p[i].free == 0)// free hai kya agar ha toh vahi de denge
		{
			//0-successfule
			ret = kfifo_alloc(&p[i].fifo, FIFO_SIZE, GFP_KERNEL);
			//allocate fifo (like message store in fifo)
			//ret successful hua toh 0 zero return karta
			if (!ret) // sucessful hua toh under jayga
			{
				spin_lock_init(&p[i].slock);// allocate spinlock to avoid read write isuue
				p[i].pid = get_current()->pid;// current pid store in pid because to maintain pid n (to know which user send that read later)
				p[i].free = 1;// available (running hai process)
				idx = i;// copy to user send karege -1 to i ki value lenga assign kiya
			}
			break;
		}
	}
	//1st process will enter here, and if all created processes havnt left
	//N starting may 0 hai
	if(idx < 0 && N+1 < MAX_USERS) // N+1 to connect extra user but it should be less that max user
	{
		ret = kfifo_alloc(&p[N].fifo, FIFO_SIZE, GFP_KERNEL);
		if (!ret) 
		{
			spin_lock_init(&p[N].slock);
			p[N].pid = get_current()->pid;
			p[N].free = 1;
			idx = N; 
			N++;
		}
	}
	// check after process created
	if(idx >= 0)
	{
		char module_buf[64];//user ko send karne
		//idx+1 = only used as our index starts from 0 only for dislpay
		sprintf(module_buf, "P%d - Joined", idx + 1);
		for(i = 0; i < N; i++)
		{
			//idx - current procees, for loop for every process
			//idx !=i to make sure joined mesaage not in same process
			//idx currwnt process same no for all process islye for loop and free bhi ho
			if(i != idx && p[i].free == 1) // message sabko jana chahiye jo khudko chood kar baki sab process jo running hai
			{
				kfifo_in_spinlocked(&p[i].fifo, module_buf, sizeof(module_buf), &p[i].slock);
			}
		}
	}
	copy_to_user((int *)arg, &idx, sizeof(int)); // current id kernal to user
	return 1;
}


static int myopen(struct inode *inode, struct file *file)
{
	//printing in kernel, seen in kernel logs
	printk("Device opened by P%d\n", get_current()->pid);
	return 0;
}


static int myclose(struct inode *inodep, struct file *filp) {
	int i, idx = -1;
	char module_buf[64];

	for(i = 0; i < N; i++)
	{
		if(p[i].pid == get_current()->pid)
		{
			kfifo_free(&p[i].fifo);
			p[i].free = 0;
			idx = i;
			sprintf(module_buf, "P%d - Left", idx + 1);
			break;
		}	
	}
	for(i = 0; i < N; i++) // for sending msg to all
	{
		if(i != idx && p[i].free == 1)
		{
			kfifo_in_spinlocked(&p[i].fifo, module_buf, sizeof(module_buf), &p[i].slock);
		}
	}
	return 0;
}


static ssize_t mywrite(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	int i, length = -1;
	char arr[MSG_LEN]; // 64 byte
	char module_buf[MSG_LEN]; // 64 byte
	
	if(copy_from_user(arr, buf, sizeof(arr))) // buffer may jo hai array arr may store hoga  //// buff = jo user process se pass kiya
	{
		return -1;
	}

	length = sprintf(module_buf, "P%d: ", arr[0] + 1);//process arr=id ////0 msg ko p1 msg karne k liye//

	
	memcpy(module_buf + length, arr + 1, 64 - length); //copy mod +len//arr = 0 + msg but 0 nhi chahiye so p1 msg chahiye // overwrite nhi ksarna hsai kark 64-length

	for(i = 0; i < N; i++)
	{
		if(i != arr[0] && p[i].free == 1) //current vale ko chood k n jo running vale sab ko send karne k liye 
		{
			kfifo_in_spinlocked(&p[i].fifo, module_buf, sizeof(module_buf), &p[i].slock);//fifo ko input de raha
		}
	}
	return 1;
}

static ssize_t myread(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	int i, ret = 0;
	for(i = 0; i < N; i++) //
	{
		if(p[i].pid == get_current()->pid)//khud ko send karna not to all islye pid 
		//agar every process ka read kaise alag hai vo get current se pata chalta hai//get current
		{
			char arr[MSG_LEN];
			int length = kfifo_out_spinlocked(&p[i].fifo, arr, sizeof(arr), &p[i].slock);//fifo se arr may dala hai
			if(length > 0) 
			{
				arr[length] = '\0';       // array k end may dala termination if by chance kisne msg send nhi kiya
				copy_to_user(buf, arr, sizeof(arr));  //fd read karna hai so buff may se arr  may dalana hai
				ret = length + 1; 
			}
			break;
		}
	} 
	return ret; 
}
// insmod ko karte he user k may jo customize read open hai vo kernal call hoga
static const struct file_operations myfops = {
    .owner					= THIS_MODULE,
    .read					= myread,
    .write					= mywrite,
    .open					= myopen,
    .release				= myclose,
    .llseek 				= no_llseek,
	.unlocked_ioctl 		= my_ioctl,
};

// rights 
struct miscdevice mydevice = 
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mihir",
    .fops = &myfops,
    .mode = S_IRUGO | S_IWUGO,
};
// charchater device intialize
static int __init my_init(void)
{
	if (misc_register(&mydevice) != 0) 
	{
		printk("device registration failed\n");
		return -1;
	}


	printk("Character device registered\n");
	return 0;
}

static void __exit my_exit(void)
{
	printk("Character device unregistered\n");
	misc_deregister(&mydevice);
}

module_init(my_init)
module_exit(my_exit)
MODULE_DESCRIPTION("Chat character device\n");
MODULE_AUTHOR("Mihir Dalvi <mdalvi3@binghamton.edu>");
MODULE_LICENSE("GPL");