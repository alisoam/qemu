#include "qemu/osdep.h"
#include "hw/char/my_uart.h"
#include "qemu/log.h"
#include "qemu/module.h"


static void my_uart_receive(void *opaque, const uint8_t *buf, int size)
{
  MyUartState *s = opaque;
  s->rxDataValid = 1;
  s->rxData = *buf;
  qemu_set_irq(s->irq, 1);
}

static void my_uart_reset(DeviceState *dev)
{
  MyUartState *s = MY_UART(dev);
  s->rxDataValid = 0;
  qemu_set_irq(s->irq, 0);
  qemu_chr_fe_accept_input(&s->chr);
}

static uint64_t my_uart_read(void *opaque, hwaddr addr, unsigned int size)
{
  MyUartState *s = opaque;
  switch (addr) {
  case 0:
    s->rxDataValid = 0;
    uint64_t retValue = s->rxData;
    qemu_set_irq(s->irq, 0);
    qemu_chr_fe_accept_input(&s->chr);
    return retValue;
  case 4:
    return s->rxDataValid;
  default:
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    return 0;
  }
  return 0;
}

static void my_uart_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
  MyUartState *s = opaque;
  uint32_t value = val64;
  unsigned char ch = value;
  switch (addr) {
  case 4:
    qemu_chr_fe_write_all(&s->chr, &ch, 1);
    return;
  default:
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
  }
}

static const MemoryRegionOps my_uart_ops = {
  .read = my_uart_read,
  .write = my_uart_write,
  .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property my_uart_properties[] = {
  DEFINE_PROP_CHR("chardev", MyUartState, chr),
  DEFINE_PROP_END_OF_LIST(),
};

static void my_uart_init(Object *obj)
{
  MyUartState *s = MY_UART(obj);
  sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
  memory_region_init_io(&s->mmio, obj, &my_uart_ops, s, TYPE_MY_UART, 0x400);
  sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void my_uart_realize(DeviceState *dev, Error **errp)
{
  MyUartState *s = MY_UART(dev);
  qemu_chr_fe_set_handlers(&s->chr, NULL, my_uart_receive, NULL, NULL, s, NULL, true);
}

static void my_uart_class_init(ObjectClass *klass, void *data)
{
  DeviceClass *dc = DEVICE_CLASS(klass);
  dc->reset = my_uart_reset;
  dc->props = my_uart_properties;
  dc->realize = my_uart_realize;
}

static const TypeInfo my_uart_info = {
  .name     = TYPE_MY_UART,
  .parent    = TYPE_SYS_BUS_DEVICE,
  .instance_size = sizeof(MyUartState),
  .instance_init = my_uart_init,
  .class_init  = my_uart_class_init,
};

static void my_uart_register_types(void)
{
  type_register_static(&my_uart_info);
}

type_init(my_uart_register_types)

