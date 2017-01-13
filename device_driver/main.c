#include "device_file.h"
#include <linux/init.h>   /* module_init, module_exit */
#include <linux/module.h> /* version info, MODULE_LICENSE, MODULE_AUTHOR, printk() */
#include <asm/io.h>       /* ioremap, ioremap_nocache, iounmap */
#include <linux/fs.h>     /* file stuff */
#include <linux/sched.h>  /* struct task_struct */

#include "../vpmu-device.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Medicine Yeh");

/*=====================================================================================*/
static int simple_driver_init(void)
{
    int result = 0;
    printk(KERN_NOTICE "VPMU: Initialization started");
    printk(KERN_NOTICE "VPMU: Offset of file.f_path.dentry(%lu), dentry.d_iname(%lu), "
                       "dentry.d_parent(%lu)\n",
           (unsigned long)offsetof(struct file, f_path)
             + (unsigned long)offsetof(struct path, dentry),
           (unsigned long)offsetof(struct dentry, d_iname),
           (unsigned long)offsetof(struct dentry, d_parent));

    printk(KERN_NOTICE "VPMU: Offset of thread_info.task(%lu), task_struct.pid(%lu)\n",
           (unsigned long)offsetof(struct thread_info, task),
           (unsigned long)offsetof(struct task_struct, pid));

#ifndef DRY_RUN
    vpmu_base = ioremap(VPMU_DEVICE_BASE_ADDR, VPMU_DEVICE_IOMEM_SIZE);
#define SET_ARG(_OFFSET, _VAL)                                                           \
    VPMU_IO_WRITE(vpmu_base + VPMU_MMAP_OFFSET_##_OFFSET, (_VAL));
    SET_ARG(FILE_f_path_dentry,
            (unsigned long)offsetof(struct file, f_path)
              + (unsigned long)offsetof(struct path, dentry));
    SET_ARG(DENTRY_d_iname, (unsigned long)offsetof(struct dentry, d_iname));
    SET_ARG(DENTRY_d_parent, (unsigned long)offsetof(struct dentry, d_parent));
    SET_ARG(THREAD_INFO_task, (unsigned long)offsetof(struct thread_info, task));
    SET_ARG(TASK_STRUCT_pid, (unsigned long)offsetof(struct task_struct, pid));
#undef SET_ARG
#endif
    result = register_device();
    return result;
}
/*-------------------------------------------------------------------------------------*/
static void simple_driver_exit(void)
{
    printk(KERN_NOTICE "VPMU: Exiting");
#ifndef DRY_RUN
    iounmap(vpmu_base);
#endif
    unregister_device();
}
/*=====================================================================================*/

module_init(simple_driver_init);
module_exit(simple_driver_exit);
