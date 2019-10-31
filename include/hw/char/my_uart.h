#ifndef HW_MY_UART_H
#define HW_MY_UART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "hw/hw.h"

#define TYPE_MY_UART "my-uart"
#define MY_UART(obj) \
  OBJECT_CHECK(MyUartState, (obj), TYPE_MY_UART)

typedef struct {
  /* <private> */
  SysBusDevice parent_obj;

  /* <public> */
  MemoryRegion mmio;

  bool rxDataValid;
  uint32_t rxData;

  CharBackend chr;
  qemu_irq irq;
} MyUartState;
#endif

