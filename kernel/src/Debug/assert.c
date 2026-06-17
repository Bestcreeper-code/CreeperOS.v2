#include "assert.h"
#include "Debug/Logger.h"
#include <stddef.h>


void __assert_fail(const char *__assertion, const char *__file,
			   unsigned int __line, const char *__function){
    
    
    sys_serial_logf("assert failed: \n", __file, __function, __line);

    sys_serial_logf("%s\n", NULL, NULL, 0,__assertion);

    for(;;);
}