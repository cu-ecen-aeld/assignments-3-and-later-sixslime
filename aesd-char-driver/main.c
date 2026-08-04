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
#include <linux/string.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("SixSlime");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /** DONE: handle open */
    filp->private_data = container_of(inode->i_cdev, struct aesd_cdev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * DONE: handle release
     */
    return 0;
}

// reads one entry at a time.
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *cbuffer = dev->circular_buffer_ptr;
    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }
    // get read entry:
    size_t read_offset = 0;
    struct aesd_aesd_buffer_entry *read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(cbuffer, *f_pos, &read_offset);
    // return if no data available:
    if (read_entry == NULL) {
        retval = 0;
        goto out;
    }
    // get read count:
    size_t read_count = count;
    size_t max_read = (read_entry->size) - read_offset;
    if (read_count > max_read) {
        read_count = max_read;
    }
    // fill output buffer:
    unsigned long uncopied = copy_to_user(buf, read_entry->buffptr[read_offset], read_count);
    read_count -= uncopied;

    // update circular buffer offset and f_pos:
    while (&cbuffer->entry[cbuffer->out_offs] != read_entry) {
        *f_pos -= cbuffer->entry[cbuffer->out_offs].size;
        cbuffer->out_offs = (cbuffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // set outs:
    *f_pos += read_count;
    retval = read_count;
    out:
    mutex_unlock(&dev->lock);
    return retval;
}

// intentionally does not use/implement fpos.
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * DONE: handle write
     */
    struct aesd_dev *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    // get sizes:
    size_t old_size = dev->current_entry.size;
    size_t new_size = old_size + count;

    // make new buffer:
    char* new_buffer = kmalloc(new_size);
    if (new_buffer == NULL) {
        retval = -ENOMEM;
        goto out;
    }

    // copy current/old buffer to new buffer:
    char* old_buffer = dev->current_entry.buffptr;
    if (old_buffer != NULL) {
        memcpy(new_buffer, old_buffer, old_size);
        free(old_buffer);
    };

    // append write to new buffer:
    unsigned long uncopied = copy_from_user(new_buffer[old_size], buf, count);

    // set outs:
    new_size -= uncopied;
    retval = count -= uncopied;
    dev->current_entry.buffptr = new_buffer;
    dev->current_entry.size = new_size;

    // add entry if newline terminated:
    if (new_buffer[new_size - 1] == '\n') {
        aesd_circular_buffer_add_entry_freeing(dev->circular_buffer_ptr, &dev->current_entry);
        dev->current_entry = new_buffer_entry();
    }

    out:
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

    /**
     * DONE: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_device.circular_buffer_ptr = kmalloc(sizeof(struct aesd_circular_buffer));
    aesd_circular_buffer_init(aesd_device.circular_buffer_ptr);
    aesd_device.current_entry = new_buffer_entry();

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

    /**
     * DONE: cleanup AESD specific poritions here as necessary
     */
    
    // free buffer:
    uint8_t index;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,aesd_device.circular_buffer_ptr,index) {
        if (entry->buffptr) {
            free(entry->buffptr);
        }
    }
    free(aesd_device.circular_buffer_ptr);

    // free hanging entry:
    if (aesd_device.current_entry.buffptr) {
        free(aesd_device.current_entry.buffptr);
    }

    // destroy mutex:
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
