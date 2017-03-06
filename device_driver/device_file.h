#ifndef DEVICE_FILE_H_
#define DEVICE_FILE_H_
#include <linux/compiler.h>  // __must_check
#include <asm/bitsperlong.h> // BITS_PER_LONG

__must_check int register_device(void); /* 0 if Ok*/

void unregister_device(void);

void vpmu_cleanup_module(int devices_to_destroy);

/*
 * Ugly macros are a way of life.
 */
#if (BITS_PER_LONG == 64)
//#pragma message("BITS_PER_LONG == 64")
#define VPMU_IO_WRITE(_addr, _value)                                                     \
    do {                                                                                 \
        *((volatile uint64_t *)(_addr)) = (const volatile uint64_t)_value;               \
    } while (0)
#define VPMU_IO_READ(_addr, _value)                                                      \
    do {                                                                                 \
        _value = *((volatile uint64_t *)(_addr));                                        \
    } while (0)

#define TARGET_WORD_SIZE 8
// End of (BITS_PER_LONG == 64)
#elif (BITS_PER_LONG == 32)
//#pragma message("BITS_PER_LONG == 32")
#define VPMU_IO_WRITE(_addr, _value)                                                     \
    do {                                                                                 \
        *((volatile uint32_t *)(_addr)) = (const volatile uint32_t)_value;               \
    } while (0)
#define VPMU_IO_READ(_addr, _value)                                                      \
    do {                                                                                 \
        _value = *((volatile uint32_t *)(_addr));                                        \
    } while (0)

#define TARGET_WORD_SIZE 4
// End of (BITS_PER_LONG == 32)
#else
#error message("BITS_PER_LONG is not defined"
               "  You may define it in this header if you need to.")
#endif

extern void *vpmu_base;
#endif // DEVICE_FILE_H_
