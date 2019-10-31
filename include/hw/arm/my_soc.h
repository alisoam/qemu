#ifndef HW_ARM_MY_SOC_H
#define HW_ARM_MY_SOC_H

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "exec/address-spaces.h"

#include "hw/arm/boot.h"
#include "hw/char/my_uart.h"

#define NUM_IRQ_LINES 64

#define TYPE_MY_SOC "my-soc"
#define MY_SOC(obj) OBJECT_CHECK(MySocState, (obj), TYPE_MY_SOC)

typedef struct {
  SysBusDevice parent_obj;

  char *cpu_type;

  ARMv7MState armv7m;

} MySocState;

#endif
