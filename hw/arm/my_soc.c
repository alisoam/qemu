#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/misc/unimp.h"
#include "cpu.h"
#include "hw/arm/my_soc.h"



#define MY_MACHINE_FLASH_SIZE (512 * 1024)
#define MY_MACHIN_SRAM1_SIZE (64 * 1024)

static void my_soc_initfn(Object *obj)
{
  MySocState *s = MY_SOC(obj);
  sysbus_init_child_obj(obj, "armv7m", &s->armv7m, sizeof(s->armv7m), TYPE_ARMV7M);
}

static void my_soc_realize(DeviceState *dev_soc, Error **errp)
{
  MySocState *s = MY_SOC(dev_soc);

  MemoryRegion *sram = g_new(MemoryRegion, 1);
  MemoryRegion *flash = g_new(MemoryRegion, 1);
  MemoryRegion *system_memory = get_system_memory();

  memory_region_init_ram(flash, NULL, "my_soc.flash", MY_MACHINE_FLASH_SIZE, &error_fatal);
  memory_region_set_readonly(flash, true);
  memory_region_add_subregion(system_memory, 0, flash);

  memory_region_init_ram(sram, NULL, "my_soc.sram", MY_MACHIN_SRAM1_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, 0x20000000, sram);

  DeviceState* armv7m = DEVICE(&s->armv7m);
  qdev_prop_set_uint32(armv7m, "num-irq", NUM_IRQ_LINES);
  qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
  qdev_prop_set_bit(armv7m, "enable-bitband", true);
  object_property_set_link(OBJECT(&s->armv7m), OBJECT(get_system_memory()), "memory", &error_abort);
  Error *err = NULL;
  object_property_set_bool(OBJECT(&s->armv7m), true, "realized", &err);
  if (err != NULL) {
    error_propagate(errp, err);
    return;
  }
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
