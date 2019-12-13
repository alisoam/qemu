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
#include "hw/arm/my_soc.h"

#define MY_MACHINE_FLASH_SIZE (512 * 1024)
#define MY_MACHIN_SRAM1_SIZE (64 * 1024)

#define NUM_IRQ_LINES 64

typedef struct {
  MySocState soc;
} MyMachineState;

// static void reset(void)
// {
//   printf("FUCK YOU ALL \n\n");
// }

static void myMachineInit(MachineState *ms)
{
  MyMachineState *s = g_new0(MyMachineState, 1);
  object_initialize_child(OBJECT(ms), "soc", &s->soc, sizeof(s->soc), TYPE_MY_SOC, &error_abort, NULL);
  object_property_set_str(OBJECT(&s->soc), ms->cpu_type, "cpu-type" , &error_abort);
  object_property_set_bool(OBJECT(&s->soc), true, "realized", &error_abort);
  armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, MY_MACHINE_FLASH_SIZE);
}

static void myMachineClassInit(MachineClass *mc)
{
  mc->desc = "This is my machine!";
  mc->init = myMachineInit;
  //  mc->reset = reset;
  mc->ignore_memory_transaction_failures = true;
  mc->no_parallel = 1;
  mc->no_floppy = 1;
  mc->no_cdrom = 1;
  mc->max_cpus = 1;
  mc->min_cpus = 1;
  mc->default_cpus = 1;
  mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m3");
}

DEFINE_MACHINE("my-machine", myMachineClassInit)
