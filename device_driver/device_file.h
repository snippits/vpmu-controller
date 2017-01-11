#ifndef DEVICE_FILE_H_
#define DEVICE_FILE_H_
#include <linux/compiler.h> /* __must_check */

__must_check int register_device(void); /* 0 if Ok*/
void unregister_device(void);

void vpmu_cleanup_module(int devices_to_destroy);

extern void *vpmu_base;
#endif // DEVICE_FILE_H_
