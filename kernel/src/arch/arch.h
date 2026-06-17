#pragma once

#include "defines/compiler_defs.h"

GCC_ATTR((noreturn)) void arch_init();

#define KERNEL_STACK_PAGE_AMOUNT 32