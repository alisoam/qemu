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

#define MY_SOC_FLASH1_ADDRESS           0x00000000
#define MY_SOC_FLASH1_SIZE              0x00100000

#define MY_SOC_FLASH2_ADDRESS           0x00100000
#define MY_SOC_FLASH2_SIZE              0x00100000

#define MY_SOC_ITCM_ADDRESS             0x10000000
#define MY_SOC_ITCM_SIZE                0x00010000

#define MY_SOC_DTCM_ADDRESS             0x20000000
#define MY_SOC_DTCM_SIZE                0x00020000

#define MY_SOC_AXI_SRAM_ADDRESS         0x24000000
#define MY_SOC_AXI_SRAM_SIZE            0x00080000

#define MY_SOC_SRAM1_ADDRESS            0x30000000
#define MY_SOC_SRAM1_SIZE               0x00020000

#define MY_SOC_SRAM2_ADDRESS            0x30020000
#define MY_SOC_SRAM2_SIZE               0x00020000

#define MY_SOC_SRAM3_ADDRESS            0x30040000
#define MY_SOC_SRAM3_SIZE               0x00008000

#define MY_SOC_SRAM4_ADDRESS            0x38000000
#define MY_SOC_SRAM4_SIZE               0x00010000

#define MY_SOC_BACKUP_SRAM_ADDRESS      0x38800000
#define MY_SOC_BACKUP_SRAM_SIZE         0x00001000

#define MY_SOC_SDRAM1_ADDRESS           0xC8000000
#define MY_SOC_SDRAM1_SIZE              0x01000000

#define MY_SOC_PERIPH_BASE              0x40000000

#define MY_SOC_UART_ADDRESS             (MY_SOC_PERIPH_BASE + 0x4000)
#define MY_SOC_DUAL_ENET_ADDRESS        (MY_SOC_PERIPH_BASE + 0x8000)

#define MY_SOC_UART_IRQ                 0
#define MY_SOC_DUAL_ENET_IRQ            1

#define TYPE_MY_SOC "my-soc"
#define MY_SOC(obj) OBJECT_CHECK(MySocState, (obj), TYPE_MY_SOC)

typedef struct {
  SysBusDevice parent_obj;

  char *cpu_type;

  ARMv7MState armv7m;
  MyUartState uart;
} MySocState;

#endif
