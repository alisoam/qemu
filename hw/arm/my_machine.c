#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/misc/unimp.h"
#include "cpu.h"

#define MY_MACHINE_FLASH_SIZE (512 * 1024)
#define MY_MACHIN_SRAM1_SIZE (64 * 1024)

#define NUM_IRQ_LINES 64


static void myMachineInit(MachineState *ms)
{
  DeviceState* nvic;
  MemoryRegion *sram = g_new(MemoryRegion, 1);
  MemoryRegion *flash = g_new(MemoryRegion, 1);
  MemoryRegion *system_memory = get_system_memory();

  memory_region_init_ram(flash, NULL, "my_machine.flash", MY_MACHINE_FLASH_SIZE, &error_fatal);
  memory_region_set_readonly(flash, true);
  memory_region_add_subregion(system_memory, 0, flash);

  memory_region_init_ram(sram, NULL, "my_machine.sram", MY_MACHIN_SRAM1_SIZE, &error_fatal);
  memory_region_add_subregion(system_memory, 0x20000000, sram);

  nvic = qdev_create(NULL, TYPE_ARMV7M);
  qdev_prop_set_uint32(nvic, "num-irq", NUM_IRQ_LINES);
  qdev_prop_set_string(nvic, "cpu-type", ms->cpu_type);
  qdev_prop_set_bit(nvic, "enable-bitband", true);
  object_property_set_link(OBJECT(nvic), OBJECT(get_system_memory()), "memory", &error_abort);
  /* This will exit with an error if the user passed us a bad cpu_type */
  qdev_init_nofail(nvic);
  armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, MY_MACHINE_FLASH_SIZE);
}

static void myMachineClassInit(MachineClass *mc)
{
  mc->desc = "This is my machine!";
  mc->init = myMachineInit;
  mc->ignore_memory_transaction_failures = true;
  mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m3");
}

DEFINE_MACHINE("my-machine", myMachineClassInit)
