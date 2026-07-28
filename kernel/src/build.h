#pragma once


#define KERNEL_NAME "CreeperKernel"

#define KERNEL_VERSION KERNEL_NAME" "__DATE__" "__TIME__
#define KERNEL_RELEASE "0.0.0"

#if defined(__x86_64__) || defined(_M_X64)
    #define MACHINE_ARCH "X86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define MACHINE_ARCH "X86"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define MACHINE_ARCH "AARCH64"
#elif defined(__arm__) || defined(_M_ARM)
    #define MACHINE_ARCH "ARM"
#else
    #error "Unsupported architecture"
#endif