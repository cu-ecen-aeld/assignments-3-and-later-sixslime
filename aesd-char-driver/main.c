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
// STUDENT:
// hell of an assignment.
// used Claude for error checking because im writing kernel C with no IDE (like a boss (idiot)).
// write code myself -> ask claude to identify errors/bugs -> implement fixes myself.

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/fs.h> // file_operations
#include <linux/string.h>
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("SixSlime");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /** DONE: handle open */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
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
     * DONE: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *cbuffer = dev->circular_buffer_ptr;
    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }
    // get read entry:
    size_t read_offset = 0;
    struct aesd_buffer_entry *read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(cbuffer, *f_pos, &read_offset);
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
    unsigned long uncopied = copy_to_user(buf, read_entry->buffptr + read_offset, read_count);
    read_count -= uncopied;

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
    char* new_buffer = kmalloc(new_size, GFP_KERNEL);
    if (new_buffer == NULL) {
        retval = -ENOMEM;
        goto out;
    }

    // copy current/old buffer to new buffer:
    const char* old_buffer = dev->current_entry.buffptr;
    if (old_buffer != NULL) {
        memcpy(new_buffer, old_buffer, old_size);
        kfree((char*)old_buffer);
    };

    // append write to new buffer:
    unsigned long uncopied = copy_from_user(new_buffer + old_size, buf, count);

    // set outs:
    new_size -= uncopied;
    retval = count -= uncopied;
    *f_pos += retval;
    dev->current_entry.buffptr = new_buffer;
    dev->current_entry.size = new_size;

    // add entry if newline terminated:
    struct aesd_circular_buffer *cbuffer = dev->circular_buffer_ptr;
    if (new_size > 0 && new_buffer[new_size - 1] == '\n') {
        if (cbuffer->entry[cbuffer->in_offs].buffptr) {
            kfree(cbuffer->entry[cbuffer->in_offs].buffptr);
        }
        aesd_circular_buffer_add_entry(dev->circular_buffer_ptr, &dev->current_entry);
        dev->current_entry = new_buffer_entry();
    }

    out:
    mutex_unlock(&dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence) {
    PDEBUG("seeking %lld bytes with whence %d",offset, whence);
    /**
     * DONE: handle seek
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *cbuffer = dev->circular_buffer_ptr;
    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }
    size_t buffer_size = 0;
    // count entire buffer if full:
    if (cbuffer->full) {
        uint8_t index;
        struct aesd_buffer_entry *entry;
        AESD_CIRCULAR_BUFFER_FOREACH(entry,cbuffer,index) {
            buffer_size += entry->size;
        }
    }
    // count until in_offs if not:
    else {
        for (size_t i = cbuffer->out_offs; i != cbuffer->in_offs; i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
            buffer_size += cbuffer->entry[i].size;
        }
    }
    out:
    mutex_unlock(&dev->lock);
    return fixed_size_llseek(filp, offset, whence, buffer_size);
}

long aesd_adjust_file_offset(struct file *filp, unsigned int cmd_index, unsigned int cmd_offset) {
    PDEBUG("ioctl seek: (%u, %u)", cmd_index, cmd_offset);
    /**
     * DONE: handle ioctl seek
     */
    if (cmd_index >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        return -EINVAL;
    }
    long retval = 0;
    loff_t out_offset = 0;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *cbuffer = dev->circular_buffer_ptr;
    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }
    // check amount of valid entries:
    uint8_t valid_entries = cbuffer->full
        ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
        : (uint8_t)((cbuffer->in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - cbuffer->out_offs)
                    % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
    if (cmd_index >= valid_entries) {
        retval = -EINVAL;
        goto out;
    }
    // entry offset:
    uint8_t entry_index = cbuffer->out_offs;
    for (unsigned int i = 0; i < cmd_index; i++) {
        out_offset += cbuffer->entry[entry_index].size;
        entry_index = (entry_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    // char offset:
    if (cmd_offset >= cbuffer->entry[entry_index].size) {
        retval = -EINVAL;
        goto out;
    }
    out_offset += cmd_offset;

    // set outs:
    filp->f_pos = out_offset;
    retval = 0;

    out:
    mutex_unlock(&dev->lock);
    return retval;
}
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    long retval = 0;
    switch(cmd) {
        case AESDCHAR_IOCSEEKTO:
            struct aesd_seekto seekto;
            if (copy_from_user(&seekto, (const void __user*)arg, sizeof(seekto)) != 0) {
                retval = -EFAULT;
            } else {
                retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
            }
            break;
        default:
            retval = -ENOTTY;
            break;
    }
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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
    aesd_device.circular_buffer_ptr = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if (aesd_device.circular_buffer_ptr == NULL) {
        return -ENOMEM;
    }
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
    
    // kfree buffer:
    uint8_t index;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,aesd_device.circular_buffer_ptr,index) {
        if (entry->buffptr) {
            kfree((char*)entry->buffptr);
        }
    }
    kfree(aesd_device.circular_buffer_ptr);

    // kfree hanging entry:
    if (aesd_device.current_entry.buffptr) {
        kfree((char*)aesd_device.current_entry.buffptr);
    }

    // destroy mutex:
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
