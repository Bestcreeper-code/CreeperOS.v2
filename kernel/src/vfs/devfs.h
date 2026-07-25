#pragma once

#include "input/input.h"
int devfs_init();
int devfs_register_input(struct input_device* idev);