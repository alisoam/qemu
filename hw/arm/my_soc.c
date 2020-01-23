#include "hw/arm/my_soc.h"

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "net/net.h"
#include "sysemu/sysemu.h"
#include "hw/misc/unimp.h"
#include "cpu.h"


static void do_sys_reset(void *opaque, int n, int level)
{
  if (level)
  {
    qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
  }
}

static void my_soc_initfn(Object *obj)
{
  MySocState *s = MY_SOC(obj);
  sysbus_init_child_obj(obj, "armv7m", &s->armv7m, sizeof(s->armv7m), TYPE_ARMV7M);
  sysbus_init_child_obj(obj, "uart", &s->uart, sizeof(s->uart), TYPE_MY_UART);
}

static void my_soc_realize(DeviceState *dev_soc, Error **errp)
{
  MySocState *s = MY_SOC(dev_soc);

  MemoryRegion *flash1 = g_new(MemoryRegion, 1);
  MemoryRegion *flash2 = g_new(MemoryRegion, 1);
  MemoryRegion *itcm = g_new(MemoryRegion, 1);
  MemoryRegion *dtcm = g_new(MemoryRegion, 1);

  MemoryRegion *axi_sram = g_new(MemoryRegion, 1);
  MemoryRegion *sram1 = g_new(MemoryRegion, 1);
  MemoryRegion *sram2 = g_new(MemoryRegion, 1);
  MemoryRegion *sram3 = g_new(MemoryRegion, 1);
  MemoryRegion *sdram1 = g_new(MemoryRegion, 1);
  MemoryRegion *system_memory = get_system_memory();

  memory_region_init_ram(flash1, NULL, "my_soc.flash1", MY_SOC_FLASH1_SIZE, &error_fatal);
  memory_region_set_readonly(flash1, true);
  memory_region_add_subregion(system_memory, MY_SOC_FLASH1_ADDRESS, flash1);

  memory_region_init_ram(flash2, NULL, "my_soc.flash2", MY_SOC_FLASH2_SIZE, &error_fatal);
  memory_region_set_readonly(flash2, true);
  memory_region_add_subregion(system_memory, MY_SOC_FLASH2_ADDRESS, flash2);

  memory_region_init_ram(itcm, NULL, "my_soc.itcm", MY_SOC_ITCM_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_ITCM_ADDRESS, itcm);
  
  memory_region_init_ram(dtcm, NULL, "my_soc.dtcm", MY_SOC_DTCM_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_DTCM_ADDRESS, dtcm);

  memory_region_init_ram(axi_sram, NULL, "my_soc.axi_sram", MY_SOC_AXI_SRAM_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_AXI_SRAM_ADDRESS, axi_sram);

  memory_region_init_ram(sram1, NULL, "my_soc.sram1", MY_SOC_SRAM1_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_SRAM1_ADDRESS, sram1);

  memory_region_init_ram(sram2, NULL, "my_soc.sram2", MY_SOC_SRAM2_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_SRAM2_ADDRESS, sram2);

  memory_region_init_ram(sram3, NULL, "my_soc.sram3", MY_SOC_SRAM3_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_SRAM1_ADDRESS, sram3);

  memory_region_init_ram(sdram1, NULL, "my_soc.sdram1", MY_SOC_SDRAM1_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, MY_SOC_SDRAM1_ADDRESS, sdram1);

  DeviceState* armv7m = DEVICE(&s->armv7m);
  qdev_prop_set_uint32(armv7m, "num-irq", MY_SOC_NUM_IRQ_LINES);
  qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
  qdev_prop_set_bit(armv7m, "enable-bitband", true);
  object_property_set_link(OBJECT(&s->armv7m), OBJECT(get_system_memory()), "memory", &error_abort);
  Error *err = NULL;
  object_property_set_bool(OBJECT(&s->armv7m), true, "realized", &err);
  if (err != NULL) {
    error_propagate(errp, err);
    return;
  }
  qdev_connect_gpio_out_named(DEVICE(&s->armv7m.nvic), "SYSRESETREQ", 0, qemu_allocate_irq(&do_sys_reset, NULL, 0));
  system_clock_scale = 1000;

  DeviceState* dev = DEVICE(&(s->uart));
  qdev_prop_set_chr(dev, "chardev", serial_hd(0));
  object_property_set_bool(OBJECT(&(s->uart)), true, "realized", &err);
  if (err != NULL) {
    error_propagate(errp, err);
    return;
  }
  SysBusDevice* busdev = SYS_BUS_DEVICE(dev);
  sysbus_mmio_map(busdev, 0, MY_SOC_UART_ADDRESS);
  sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, MY_SOC_UART_IRQ));

  qemu_check_nic_model(&nd_table[0], "my_soc");
  DeviceState* enet = qdev_create(NULL, "my_dual_enet");
  qdev_set_nic_properties(enet, &nd_table[0]);
  qdev_init_nofail(enet);
  sysbus_mmio_map(SYS_BUS_DEVICE(enet), 0, MY_SOC_DUAL_ENET_ADDRESS);
  sysbus_connect_irq(SYS_BUS_DEVICE(enet), 0, qdev_get_gpio_in(armv7m, MY_SOC_DUAL_ENET_IRQ));
}

static Property my_soc_properties[] = {
    DEFINE_PROP_STRING("cpu-type", MySocState, cpu_type),
    DEFINE_PROP_END_OF_LIST(),
};

static void my_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = my_soc_realize;
    dc->props = my_soc_properties;
}

static const TypeInfo my_soc_info = {
    .name          = TYPE_MY_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MySocState),
    .instance_init = my_soc_initfn,
    .class_init    = my_soc_class_init,
};

static void my_soc_types(void)
{
    type_register_static(&my_soc_info);
}

type_init(my_soc_types)
