#include "device_file.h"
#include <linux/fs.h>     /* file stuff */
#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/module.h> /* THIS_MODULE */
#include <linux/cdev.h>   /* char device stuff */
#include <asm/uaccess.h>  /* copy_to_user() */
#include <asm/io.h>       /* ioremap(), ioremap_nocache(), iounmap() */
#include <linux/slab.h>   /* kzalloc() */
#include <linux/device.h> /* class_create(), device_create() */
#include <linux/mutex.h>  /* mutex stuff */

#include "../vpmu-device.h" /* VPMU Configurations */
/* Maxumum length of a block that can be read or written in one operation */
#ifndef VPMU_DEVICE_BLOCK_SIZE
#define VPMU_DEVICE_BLOCK_SIZE 512
#endif

#define VPMU_CDEVICE_NAME "vpmu-device"

#ifdef DRY_RUN
#pragma message "DRY_RUN is defined. Compiled with dry run!!"
#endif

/* Device handler for all states and data */
struct vpmu_dev {
    unsigned char *data;
    unsigned long  buffer_size;
    unsigned long  block_size;
    struct mutex   cfake_mutex;
    struct cdev    cdev;
};

/* Sample String */
static const char    g_s_Hello_World_string[] = "Hello world from VPMU!\n\0";
static const ssize_t g_s_Hello_World_size     = sizeof(g_s_Hello_World_string);

void *                  vpmu_base    = NULL;
static struct vpmu_dev *vpmu_devices = NULL;
static struct class *   vpmu_class   = NULL;

static int vpmu_major_number = 0;
static int vpmu_ndevices     = 1;

module_param(vpmu_ndevices, int, S_IRUGO);

/*===============================================================================================*/
static ssize_t device_file_read(struct file *file_ptr,
                                char __user *user_buffer,
                                size_t       count,
                                loff_t *     possition)
{
    printk(KERN_NOTICE "VPMU: Device file %s is read at offset = %i, read bytes count = %u",
            file_ptr->f_path.dentry->d_parent->d_iname,
           (int)*possition,
           (unsigned int)count);

    if (*possition >= g_s_Hello_World_size) return 0;

    if (*possition + count > g_s_Hello_World_size)
        count = g_s_Hello_World_size - *possition;

    if (copy_to_user(user_buffer, g_s_Hello_World_string + *possition, count) != 0)
        return -EFAULT;

    *possition += count;

#ifndef DRY_RUN
    // user_buffer[*possition] = ioread32(vpmu_base + *possition);
#endif
    return count;
}

static ssize_t device_file_write(struct file *file_ptr,
                                 const char * user_buffer,
                                 size_t       count,
                                 loff_t *     possition)
{
    printk(KERN_NOTICE
           "VPMU: Device file %s is write at offset = %i, write bytes count = %u",
           file_ptr->f_path.dentry->d_iname,
           (int)*possition,
           (unsigned int)count);

#ifndef DRY_RUN
    iowrite32(*possition, vpmu_base);
#endif
    return count;
}

static int device_file_open(struct inode *inode, struct file *file_ptr)
{
    printk(KERN_NOTICE "VPMU: Device %s Open", file_ptr->f_path.dentry->d_iname);
    return 0;
}

static int device_file_release(struct inode *inode, struct file *file_ptr)
{
    printk(KERN_NOTICE "VPMU: Device Release");
    return 0;
}

/*===============================================================================================*/
static struct file_operations simple_driver_fops = {.owner   = THIS_MODULE,
                                                    .read    = device_file_read,
                                                    .write   = device_file_write,
                                                    .open    = device_file_open,
                                                    .release = device_file_release};

/* ================================================================ */
/* Setup and register the device with specific index (the index is also
 * the minor number of the device).
 * Device class should be created beforehand.
 */
static int vpmu_construct_device(struct vpmu_dev *dev, int minor, struct class *class)
{
    int            err    = 0;
    dev_t          devno  = MKDEV(vpmu_major_number, minor);
    struct device *device = NULL;

    BUG_ON(dev == NULL || class == NULL);

    /* Memory is to be allocated when the device is opened the first time */
    dev->data        = NULL;
    dev->buffer_size = VPMU_DEVICE_IOMEM_SIZE;
    dev->block_size  = VPMU_DEVICE_BLOCK_SIZE;
    mutex_init(&dev->cfake_mutex);

    cdev_init(&dev->cdev, &simple_driver_fops);
    dev->cdev.owner = THIS_MODULE;

    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_WARNING "VPMU: Error %d while trying to add %s%d",
               err,
               VPMU_CDEVICE_NAME,
               minor);
        return err;
    }

    device = device_create(class,
                           NULL, /* no parent device */
                           devno,
                           NULL, /* no additional data */
                           VPMU_CDEVICE_NAME "-%d",
                           minor);

    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        printk(KERN_WARNING "VPMU: Error %d while trying to create %s-%d",
               err,
               VPMU_CDEVICE_NAME,
               minor);
        cdev_del(&dev->cdev);
        return err;
    }
    return 0;
}

/*===============================================================================================*/
static char *vpmu_devnode(struct device *dev, umode_t *mode)
{
    if (!mode) return NULL;
    if (dev->devt == MKDEV(vpmu_major_number, 0)
        || dev->devt == MKDEV(vpmu_major_number, 2))
        *mode = 0666;
    return NULL;
}

int register_device(void)
{
    int   i;
    int   err                = 0;
    dev_t dev                = 0;
    int   devices_to_destroy = 0;

    printk(KERN_NOTICE "VPMU: register_device() is called.");

    if (vpmu_ndevices <= 0) {
        printk(KERN_WARNING "VPMU: Invalid value of cfake_ndevices: %d\n", vpmu_ndevices);
        err = -EINVAL;
        return err;
    }

    /* Get a range of minor numbers (starting with 0) to work with */
    err = alloc_chrdev_region(&dev, 0, vpmu_ndevices, VPMU_CDEVICE_NAME);
    if (err < 0) {
        printk(KERN_WARNING "VPMU: alloc_chrdev_region() failed\n");
        return err;
    }
    vpmu_major_number = MAJOR(dev);

    printk(KERN_NOTICE "VPMU: registered character device with major number = "
                       "%i and minor numbers 0...%d",
           vpmu_major_number,
           vpmu_ndevices);

    /* Create device class (before allocation of the array of devices) */
    vpmu_class = class_create(THIS_MODULE, VPMU_CDEVICE_NAME);
    if (IS_ERR(vpmu_class)) {
        err = PTR_ERR(vpmu_class);
        goto fail;
    }
    vpmu_class->devnode = vpmu_devnode;

    /* Allocate the array of devices */
    vpmu_devices =
      (struct vpmu_dev *)kzalloc(vpmu_ndevices * sizeof(struct vpmu_dev), GFP_KERNEL);
    if (vpmu_devices == NULL) {
        err = -ENOMEM;
        goto fail;
    }

    /* Construct devices */
    for (i = 0; i < vpmu_ndevices; ++i) {
        err = vpmu_construct_device(&vpmu_devices[0], 0, vpmu_class);
        if (err) {
            devices_to_destroy = i;
            goto fail;
        }
    }

    return 0; // success
fail:
    vpmu_cleanup_module(devices_to_destroy);
    return 0;
}
/*-----------------------------------------------------------------------------------------------*/
void unregister_device(void)
{
    printk(KERN_NOTICE "VPMU: unregister_device() is called");
    vpmu_cleanup_module(vpmu_ndevices);
}

/* Destroy the device and free its buffer */
static void vpmu_destroy_device(struct vpmu_dev *dev, int minor, struct class *class)
{
    BUG_ON(dev == NULL || class == NULL);
    device_destroy(class, MKDEV(vpmu_major_number, minor));
    cdev_del(&dev->cdev);
    kfree(dev->data);
    mutex_destroy(&dev->cfake_mutex);
    return;
}

void vpmu_cleanup_module(int devices_to_destroy)
{
    int i;

    /* Get rid of character devices (if any exist) */
    if (vpmu_devices) {
        for (i = 0; i < devices_to_destroy; ++i) {
            vpmu_destroy_device(&vpmu_devices[i], i, vpmu_class);
        }
        kfree(vpmu_devices);
    }

    if (vpmu_class) class_destroy(vpmu_class);

    /* [NB] vpmu_cleanup_module is never called if alloc_chrdev_region() has failed. */
    if (vpmu_major_number)
        unregister_chrdev_region(MKDEV(vpmu_major_number, 0), vpmu_ndevices);
    return;
}
