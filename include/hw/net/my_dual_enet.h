#ifndef MY_DUAL_ENET_H
#define MY_DUAL_ENET_H

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "net/net.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include <zlib.h>

//#define DEBUG_MY_DUAL_ENET 1

#ifdef DEBUG_MY_DUAL_ENET
#define DPRINTF(fmt, ...) \
do { printf("my_dual_enet: " fmt , ## __VA_ARGS__); } while (0)
#define BADF(fmt, ...) \
do { fprintf(stderr, "mt_enet: error: " fmt , ## __VA_ARGS__); exit(1);} while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#define BADF(fmt, ...) \
do { fprintf(stderr, "my_dual_enet: error: " fmt , ## __VA_ARGS__);} while (0)
#endif


#define MY_DUAL_ENET_MAC1_REGISTER_OFFSET                    0x000
#define MY_DUAL_ENET_MAC2_REGISTER_OFFSET                    0x004

#define MY_DUAL_ENET_COMMAND_REGISTER_OFFSET                 0x100
#define MY_DUAL_ENET_CONTROL_REGISTER_OFFSET                 0x104
#define MY_DUAL_ENET_RX_DESCRIPTOR_REGISTER_OFFSET           0x108
#define MY_DUAL_ENET_RX_STATUS_REGISTER_OFFSET               0x10C
#define MY_DUAL_ENET_RX_DESCRIPTOR_NUMBER_REGISTER_OFFSET    0x110
#define MY_DUAL_ENET_RX_PRODUCE_INDEX_REGISTER_OFFSET        0x114
#define MY_DUAL_ENET_RX_CONSUME_INDEX_REGISTER_OFFSET        0x118
#define MY_DUAL_ENET_TX_DESCRIPTOR_REGISTER_OFFSET           0x11C
#define MY_DUAL_ENET_TX_STATUS_REGISTER_OFFSET               0x120
#define MY_DUAL_ENET_TX_DESCRIPTOR_NUMBER_REGISTER_OFFSET    0x124
#define MY_DUAL_ENET_TX_PRODUCE_INDEX_REGISTER_OFFSET        0x128
#define MY_DUAL_ENET_TX_CONSUME_INDEX_REGISTER_OFFSET        0x12C

#define MY_DUAL_ENET_INT_STATUS_REGISTER_OFFSET              0xFE0
#define MY_DUAL_ENET_INT_ENABLE_REGISTER_OFFSET              0xFE4
#define MY_DUAL_ENET_INT_CLEAR_REGISTER_OFFSET               0xFE8
#define MY_DUAL_ENET_TX_BUFFER_SIZE                         (2048 + 4)

#define NIC_PORTS                                       2

#define TYPE_MY_DUAL_ENET "my_dual_enet"
#define MY_DUAL_ENET(obj) \
    OBJECT_CHECK(my_dual_enet_state, (obj), TYPE_MY_DUAL_ENET)

typedef struct {
    SysBusDevice parent_obj;

    bool rxEnable;
    uint32_t rxDescriptor;
    uint32_t rxStatus;
    uint32_t rxDescriptorNumber;
    uint32_t rxProduceIndex;
    uint32_t rxConsumeIndex;

    bool txEnable;
    uint32_t txDescriptor;
    uint32_t txStatus;
    uint32_t txDescriptorNumber;
    uint32_t txProduceIndex;
    uint32_t txConsumeIndex;
    uint8_t txBuffer[MY_DUAL_ENET_TX_BUFFER_SIZE];
    uint32_t txBufferIndex;
    uint32_t irqStatus;
    uint32_t irqEnable;

    NICState *nic[NIC_PORTS];
    NICConf conf[NIC_PORTS];
    MACAddr macaddr;
    qemu_irq irq;
    MemoryRegion mmio;
} my_dual_enet_state;

void qdev_set_dual_enet_nic_properties(DeviceState *dev, NICInfo *nd0, NICInfo *nd1);

#endif