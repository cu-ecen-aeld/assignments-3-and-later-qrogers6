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
  PDEBUG("open");

  filp->private_data = &aesd_device;

  if(mutex_lock_interruptible(&aesd_device.lock))
  {
    return -ERESTARTSYS;
  }

  mutex_unlock(&aesd_device.lock);
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
  ssize_t bytes = 0;
  ssize_t firstEntryPos = 0;
  struct aesd_dev *dev = filp->private_data;
  char *temp_ptr = NULL;
  struct aesd_buffer_entry *entry = NULL;

  PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

  if(mutex_lock_interruptible(&dev->lock))
  {
    PDEBUG("mutex_lock_interruptible()");
    return retval;
  }

  entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cbuffer, *f_pos, &firstEntryPos);
  if(!entry)
  {
    PDEBUG("aesd_circular_buffer_find_entry_offset_for_fpos()");
    retval = 0;
    *f_pos = 0;
    mutex_unlock(&dev->lock);
    return retval;
  }

  bytes = entry->size - firstEntryPos;
  if(copy_to_user(buf, entry->buffptr + firstEntryPos, bytes))
  {
    PDEBUG("copy_to_user()");
    retval = -EFAULT;
    mutex_unlock(&dev->lock);
    return retval;
  }

  count -= bytes;
  temp_ptr = (char *)krealloc(temp_ptr, entry->size + 1, GFP_KERNEL);
  temp_ptr = memcpy(temp_ptr, entry->buffptr + firstEntryPos, entry->size);
  temp_ptr[entry->size] = '\0';
  kfree(temp_ptr);

  *f_pos += bytes;
  retval = bytes;
  mutex_unlock(&dev->lock);
  return retval;
}

/*ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    return retval;
}*/

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    static char *temp_buffer = NULL;
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld\n", count, *f_pos);
    /**
     * TODO: handle write
     */
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (dev->n_entries == (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED + 1))
    {
        dev->n_entries = 0;
        dev->full_buffer_size = 0;
        kfree(dev->full_buffer);
        dev->last_entry.buffptr = NULL;
        dev->last_entry.size = 0;
        dev->full_buffer = NULL;
        temp_buffer = NULL;
        aesd_circular_buffer_init(&dev->cbuffer);
    }
    dev->full_buffer_size += count;
    dev->full_buffer = (const char *)krealloc(dev->full_buffer, dev->full_buffer_size, GFP_KERNEL);
    if (!dev->full_buffer)
    {
        retval = -ENOMEM;
        goto out;
    }
    if (!temp_buffer)
    {
        temp_buffer = dev->full_buffer;
    }
    else if (temp_buffer != dev->full_buffer)
    {
        int l_n = 0;
        ssize_t l_s = 0;
        while (1)
        {
            if ((l_n != AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) && (dev->cbuffer.entry[l_n].size))
            {
                dev->cbuffer.entry[l_n].buffptr = dev->full_buffer + l_s;
            }
            else
            {
                break;
            }
            l_s += dev->cbuffer.entry[l_n].size;
            l_n++;
        }
        temp_buffer = dev->full_buffer;
    }

    if (dev->n_entries == 0) // no_entries_set_first_entry
    {
        dev->last_entry.buffptr = dev->full_buffer;
    }
    else if (dev->last_entry.buffptr == NULL)
    { // NEW_ENTRY_after\n;
        dev->last_entry.buffptr = dev->full_buffer + dev->full_buffer_size - count;
        dev->last_entry.size = 0;
    }

    copy_from_user(dev->last_entry.buffptr + dev->last_entry.size, buf, count);
    dev->last_entry.size += count;

    if (dev->last_entry.buffptr[dev->last_entry.size - 1] == '\n')
    {
        aesd_circular_buffer_add_entry(&dev->cbuffer, &dev->last_entry);
        dev->last_entry.buffptr = NULL;
        dev->last_entry.size = 0;
        dev->n_entries++;
    }
    retval = count;
out:

    if (retval > 0)
    {
        ssize_t n = 0;
        char *temp_ptr = NULL;
        while (n < dev->n_entries && n <= 9)
        {
            temp_ptr = (char *)krealloc(temp_ptr, dev->cbuffer.entry[n].size + 1, GFP_KERNEL);
            temp_ptr = memcpy(temp_ptr, dev->cbuffer.entry[n].buffptr, dev->cbuffer.entry[n].size);
            temp_ptr[dev->cbuffer.entry[n].size] = '\0';
            PDEBUG("buffer content: Entry[%d]:  %s - whith sizeof [%zu] BYTES \n", n, temp_ptr, dev->cbuffer.entry[n].size);
            n++;
        }
        kfree(temp_ptr);
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
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    if(aesd_device.full_buffer)
    {
      kfree(aesd_device.full_buffer);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
