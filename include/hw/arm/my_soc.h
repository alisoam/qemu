#ifndef HW_ARM_MY_SOC_H
#define HW_ARM_MY_SOC_H

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "exec/address-spaces.h"

#include "hw/arm/boot.h"
#include "hw/arm/armv7m.h"
#include "hw/char/my_uart.h"

#define MY_SOC_NUM_IRQ_LINES  64

#define MY_SOC_FLASH_SIZE     (512 * 1024)

#define MY_SOC_SRAM1_SIZE     (64 * 1024)
#define MY_SOC_SRAM1_ADDRESS  0x10000000
#define MY_SOC_SRAM1_ADDRESS  0x10000000

#define MY_SOC_SRAM2_SIZE     (64 * 1024)
#define MY_SOC_SRAM2_ADDRESS  0x20000000
#define MY_SOC_SRAM2_ADDRESS  0x20000000

#define MY_SOC_PERIPH_BASE      (0x40000000)

#define MY_SOC_UART_ADDRESS        (MY_SOC_PERIPH_BASE + 0x4000)
#define MY_SOC_ENET_ADDRESS        (MY_SOC_PERIPH_BASE + 0x8000)

#define MY_SOC_UART_IRQ       0
#define MY_SOC_ENET_IRQ       1

#define TYPE_MY_SOC "my-soc"
#define MY_SOC(obj) OBJECT_CHECK(MySocState, (obj), TYPE_MY_SOC)

typedef struct {
  SysBusDevice parent_obj;

  char *cpu_type;

  ARMv7MState armv7m;
  MyUartState uart;
} MySocState;

#endif
