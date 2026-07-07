#pragma once

#define _gcc_kernel_address_space 1
#define _gcc_user_address_space 3

#define GCC_ATTR(attrib) __attribute__(attrib)

#define _GCC_ADDR_SPACE(N) GCC_ATTR((address_space(N)))
#define _GCC_SECTION(sec) GCC_ATTR((section(sec)))

#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define arr_lengthof(comptime_array) (sizeof(comptime_array)/sizeof(comptime_array[0]))



#define __user _GCC_ADDR_SPACE(_gcc_user_address_space)

static inline int copy_from_user(
    void *to,
    const volatile void __user *from,
    unsigned long n
) {
    volatile char *dst = to;
    const volatile char __user *src = from;

    while (n--) *dst++ = *src++;

    return 0;
}

static inline int copy_to_user(
    volatile void __user *to,
    const void *from,
    unsigned long n
) {
    volatile char __user *dst = to;
    const volatile char *src = from;

    while (n--)
        *dst++ = *src++;

    return 0;
}