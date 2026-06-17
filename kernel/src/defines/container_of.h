#pragma once
#include "compiler_defs.h"
#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	_Static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })

	 