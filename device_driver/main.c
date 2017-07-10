#include "device_file.h"
#include <linux/init.h>     /* module_init, module_exit */
#include <linux/module.h>   /* version info, MODULE_LICENSE, MODULE_AUTHOR, printk() */
#include <asm/io.h>         /* ioremap, ioremap_nocache, iounmap */
#include <linux/fs.h>       /* file stuff */
#include <linux/sched.h>    /* struct task_struct */
#include <asm/uaccess.h>    /* Needed by segment descriptors */
#include <linux/kallsyms.h> /* kallsyms_lookup_name() */
#include <linux/version.h>

#include "../vpmu-device.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Medicine Yeh");

/* In 2.2.3 /usr/include/linux/version.h includes a
 * macro for this, but 2.0.35 doesn't - so I add it
 * here if necessary.
 */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a, b, c) ((a << 16) + (b << 8) + (c))
#endif

/* Lookup the address for this symbol. Returns 0 if not found. */

#define SET_ARG(_OFFSET, _VAL)                                                           \
    VPMU_IO_WRITE(vpmu_base + VPMU_MMAP_OFFSET_##_OFFSET, (_VAL));

static int start_with(const char *str, const char *prefix)
{
    return (strncmp(str, prefix, strlen(prefix)) == 0);
}

typedef struct KallsymsData {
    unsigned long addr;
    const char *  prefix_name;
    const char *  name;
} KallsymsData;

static int find_fn(void *data, const char *name, struct module *mod, unsigned long addr)
{
    KallsymsData *ptr = (KallsymsData *)data;

    // printk(KERN_DEBUG "%s\n", name);
    if (name != NULL && start_with(name, ptr->prefix_name)) {
        ptr->addr = addr;
        ptr->name = name;
        return 1;
    }

    return 0;
}

int pass_kernel_symbol(const char *name)
{
    unsigned long ret;

    ret = kallsyms_lookup_name(name);
    if (ret == 0) {
        printk(KERN_DEBUG "VPMU: Symbol %s is not found\n", name);
        return 0;
    } else {
        printk(KERN_DEBUG "VPMU: Symbol %s : %lx\n", name, ret);
#ifndef DRY_RUN
        SET_ARG(KERNEL_SYM_NAME, name);
        SET_ARG(KERNEL_SYM_ADDR, ret);
#endif
    }

    return 1;
}

int pass_kernel_symbol_prefix(const char *prefix_name)
{
    KallsymsData ret = {};
    ret.prefix_name  = prefix_name;

    kallsyms_on_each_symbol(find_fn, (void *)&ret);
    if (ret.addr == 0) {
        printk(KERN_DEBUG "VPMU: Symbol (prefix) %s is not found\n", prefix_name);
        return 0;
    } else {
        printk(KERN_DEBUG "VPMU: Symbol (prefix) %s : %lx\n", prefix_name, ret.addr);
#ifndef DRY_RUN
        SET_ARG(KERNEL_SYM_NAME, prefix_name);
        SET_ARG(KERNEL_SYM_ADDR, ret.addr);
#endif
    }

    return 1;
}

/*=====================================================================================*/
static int simple_driver_init(void)
{
    // Offsets required by VPMU Event Tracing
    unsigned long offset_dentry   = 0;
    unsigned long offset_d_iname  = 0;
    unsigned long offset_d_parent = 0;
    unsigned long offset_task     = 0;
    unsigned long offset_pid      = 0;
    // A temporary variable for storing results (return values)
    int result = 0;

    printk(KERN_DEBUG "VPMU: Initialization started\n");

    printk(KERN_DEBUG "VPMU: Running on Linux version %u:%u:%u\n",
           (LINUX_VERSION_CODE >> 16) & 0xff,
           (LINUX_VERSION_CODE >> 8) & 0xff,
           (LINUX_VERSION_CODE >> 0) & 0xff);

    // Retrieve structure offset through compiler's help
    offset_dentry = (unsigned long)offsetof(struct file, f_path)
                    + (unsigned long)offsetof(struct path, dentry);
    offset_d_iname  = (unsigned long)offsetof(struct dentry, d_iname);
    offset_d_parent = (unsigned long)offsetof(struct dentry, d_parent);
    offset_pid      = (unsigned long)offsetof(struct task_struct, pid);

// Retrieve architecture/version dependent information
#if defined(__arm__)
    // ARM specific parameters
    offset_task = (unsigned long)offsetof(struct thread_info, task);

// End of ARM specific parameters
#else
// X86 specific parameters

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
    // X86 and kernel < 4.9.0
    offset_task = (unsigned long)offsetof(struct thread_info, task);

// End of X86 and kernel < 4.9.0
#endif
// End of X86 specific parameters
#endif

    // Show debug messages
    printk(KERN_DEBUG "VPMU: Offset of file.f_path.dentry(%lu), dentry.d_iname(%lu), "
                      "dentry.d_parent(%lu)\n",
           offset_dentry,
           offset_d_iname,
           offset_d_parent);

    printk(KERN_DEBUG "VPMU: Offset of thread_info.task(%lu), task_struct.pid(%lu)\n",
           offset_task,
           offset_pid);

#ifndef DRY_RUN
    vpmu_base = ioremap(VPMU_DEVICE_BASE_ADDR, VPMU_DEVICE_IOMEM_SIZE);

    // Pass structure offset information to VPMU
    SET_ARG(FILE_f_path_dentry, offset_dentry);
    SET_ARG(DENTRY_d_iname, offset_d_iname);
    SET_ARG(DENTRY_d_parent, offset_d_parent);
    SET_ARG(THREAD_INFO_task, offset_task);
    SET_ARG(TASK_STRUCT_pid, offset_pid);

    SET_ARG(LINUX_VERSION, (unsigned long)LINUX_VERSION_CODE);
    VPMU_IO_WRITE(vpmu_base + VPMU_MMAP_THREAD_SIZE, (THREAD_SIZE));
#endif

    // Pass kernel symbol address information to VPMU
    pass_kernel_symbol("mmap_region");
    pass_kernel_symbol("mprotect_fixup");
    pass_kernel_symbol("unmap_region");
    if (!pass_kernel_symbol("_do_fork")) pass_kernel_symbol("do_fork");
    pass_kernel_symbol("wake_up_new_task");
    pass_kernel_symbol("do_exit");
    pass_kernel_symbol("__switch_to");
    if (!pass_kernel_symbol_prefix("do_execveat_common")) {
        pass_kernel_symbol_prefix("do_execve_common");
    }

    result = register_device();
    return result;
}
/*-------------------------------------------------------------------------------------*/
static void simple_driver_exit(void)
{
    printk(KERN_DEBUG "VPMU: Exiting\n");
#ifndef DRY_RUN
    iounmap(vpmu_base);
#endif
    unregister_device();
}
/*=====================================================================================*/

#undef SET_ARG
module_init(simple_driver_init);
module_exit(simple_driver_exit);
