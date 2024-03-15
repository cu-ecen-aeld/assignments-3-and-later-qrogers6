/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Quincy Rogers");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
  struct aesd_dev *dev = NULL;

  PDEBUG("open");
  dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
  filp->private_data = dev;
  return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
  PDEBUG("release");
  return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
  ssize_t retval = 0;
  struct aesd_dev *dev;

  struct aesd_buffer_entry *read_entry = NULL;
  ssize_t offset = 0;
  ssize_t bytes = 0;

  PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

  dev = (struct aesd_dev*) filp->private_data;

  if(filp == NULL || buf == NULL || f_pos == NULL)
  {
    return -EFAULT; 
  }
  
  if(mutex_lock_interruptible(&dev->lock))
  {
    PDEBUG("mutex_lock_interruptible()");
    return -ERESTARTSYS;
  }

  read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->buffer), *f_pos, &offset); 
  if(read_entry == NULL)
  {
    mutex_unlock(&(dev->lock));
    return retval;
  }
  else
  {
    if(count > (read_entry->size - offset))
    {
      count = read_entry->size - offset;
    }
    
  }

  bytes = copy_to_user(buf, (read_entry->buffptr + offset), count);
  
  retval = count - bytes;
  *f_pos += retval;

  mutex_unlock(&(dev->lock));

  return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{ 
  struct aesd_dev *dev;
  const char *entry = NULL;
  ssize_t retval = -ENOMEM;
  ssize_t bytes = 0;
  PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
  
  if(count == 0)
  {
    return 0;
  }

  if(filp  == NULL  || 
     buf   == NULL  || 
     f_pos == NULL)
  {
    return -EFAULT;
  }

  dev = (struct aesd_dev*) filp->private_data;

  if(mutex_lock_interruptible(&dev->lock))
  {
    PDEBUG("mutex_lock_interruptible()");
    return -ERESTARTSYS;
  }
  
  if(dev->entry.size == 0)
  {  
    dev->entry.buffptr = kmalloc(count*sizeof(char), GFP_KERNEL);

    if(dev->entry.buffptr == NULL)
    {
      PDEBUG("kmalloc()");
      mutex_unlock(&dev->lock);
      return retval;
    }
  }
  else
  {
    dev->entry.buffptr = krealloc(dev->entry.buffptr, (dev->entry.size + count)*sizeof(char), GFP_KERNEL);

    if(dev->entry.buffptr == NULL)
    {
      PDEBUG("kmalloc()");
      mutex_unlock(&dev->lock);
      return retval;
    }
  }

  bytes = copy_from_user((void *)(dev->entry.buffptr + dev->entry.size), buf, count);
  retval = count - bytes;
  dev->entry.size += retval;

  if(memchr(dev->entry.buffptr, '\n', dev->entry.size))
  {
    entry = aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry); 

    if(entry == NULL)
    {
      kfree(entry);
    }

    dev->entry.buffptr = NULL;
    dev->entry.size = 0;
  }

  mutex_unlock(&dev->lock);
  return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.lock); 
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
  uint8_t index;
  struct aesd_buffer_entry *entry = NULL;

  dev_t devno = MKDEV(aesd_major, aesd_minor);

  cdev_del(&aesd_device.cdev);
  
  AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) 
  {
    kfree(entry->buffptr);
  }

  mutex_destroy(&aesd_device.lock);

  unregister_chrdev_region(devno, 1);
}   



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
