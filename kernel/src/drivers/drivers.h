#pragma once
#include "Debug/Logger.h"
#include "defines/compiler_defs.h"

typedef struct {
    const char *name;
    int (*init)(void);
} init_driver_t;



#define REGISTER_DRIVER_PHASE(d_name, init_f, phase) \
    GCC_ATTR((section(#phase "_drivers"), used)) \
    static init_driver_t __##d_name##_driver_struct = {.name=#d_name, .init=init_f}


#define REGISTER_DRIVER_CORE(d_name, init_f)  REGISTER_DRIVER_PHASE(d_name, init_f, core)
#define REGISTER_DRIVER_DEV(d_name, init_f)   REGISTER_DRIVER_PHASE(d_name, init_f, dev)
#define REGISTER_DRIVER_FS(d_name, init_f)    REGISTER_DRIVER_PHASE(d_name, init_f, fs)
#define REGISTER_DRIVER_LATE(d_name, init_f)  REGISTER_DRIVER_PHASE(d_name, init_f, late)


extern init_driver_t __start_core_drivers[], __stop_core_drivers[];
extern init_driver_t __start_dev_drivers[], __stop_dev_drivers[];
extern init_driver_t __start_fs_drivers[], __stop_fs_drivers[];
extern init_driver_t __start_late_drivers[], __stop_late_drivers[];


int init_drivers(init_driver_t *start, init_driver_t *stop);

#define core_init() \
    Sys_Info("initing core drivers\n"); \
    init_drivers(__start_core_drivers, __stop_core_drivers); \
    Sys_Success("inited core drivers\n")

#define dev_init() \
    Sys_Info("initing dev drivers\n"); \
    init_drivers(__start_dev_drivers, __stop_dev_drivers); \
    Sys_Success("inited dev drivers\n")

#define fs_init() \
    Sys_Info("initing fs drivers\n"); \
    init_drivers(__start_fs_drivers, __stop_fs_drivers); \
    Sys_Success("inited fs drivers\n")

#define late_init() \
    Sys_Info("initing late drivers\n"); \
    init_drivers(__start_late_drivers, __stop_late_drivers); \
    Sys_Success("inited late drivers\n")