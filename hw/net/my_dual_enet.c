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
    uint32_t txBuffer[2048];
    uint32_t irqStatus;
    uint32_t irqEnable;

    NICState *nic[NIC_PORTS];
    NICConf conf[NIC_PORTS];
    qemu_irq irq;
    MemoryRegion mmio;
} my_dual_enet_state;

static void my_dual_enet_reset_rx(my_dual_enet_state *s)
{
    s->rxEnable = false;
    s->rxDescriptorNumber = 0;
    s->rxStatus = 0;
    s->rxProduceIndex = 0;
    s->rxConsumeIndex = 0;
    s->irqStatus &= ~(1UL << 2 | 1UL << 3);
}

static void my_dual_enet_reset_tx(my_dual_enet_state *s)
{
    s->txEnable = false;
    s->txDescriptorNumber = 0;
    s->txStatus = 0;
    s->txProduceIndex = 0;
    s->txConsumeIndex = 0;
    s->irqStatus &= ~(1UL << 6 | 1UL << 7);
}

static int my_dual_enet_can_receive(NetClientState *nc)
{
    my_dual_enet_state *s = qemu_get_nic_opaque(nc);

    if (!s->rxEnable ||
        s->rxProduceIndex == (s->rxConsumeIndex + s->rxDescriptorNumber) % (s->rxDescriptorNumber + 1))
        return 0;
    return 1;
}

static ssize_t my_dual_enet_receive(NetClientState *nc, const uint8_t *buf, size_t size, unsigned int port)
{
    if (size == 0)
        return 0;

    uint32_t crc = crc32(~0, buf, size);
    my_dual_enet_state *s = qemu_get_nic_opaque(nc);

    uint32_t descriptor_address = s->rxDescriptor + s->rxProduceIndex * 8;
    uint32_t control_address = descriptor_address + 4;
    uint32_t status_address = s->rxStatus + s->rxProduceIndex * 8;

    uint32_t buffer_address;
    address_space_read(&address_space_memory, descriptor_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&buffer_address, 4);
    // buffer_address = bswap32(buffer_address);
    uint32_t control;
    address_space_read(&address_space_memory, control_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&control, 4);
    // control = bswap32(control);

    size_t buffer_size = (control & 0x7FF) + 1;
    size_t bytes_to_receive = size + 8;
    // bytes_to_receive = bswap32(bytes_to_receive);
    if (buffer_size >= bytes_to_receive)
    {
        address_space_write(&address_space_memory, buffer_address, MEMTXATTRS_UNSPECIFIED, buf, size);
        address_space_write(&address_space_memory, buffer_address + size, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&crc, 4);
        address_space_write(&address_space_memory + 4, buffer_address + size, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&crc, 4);
    }
    // TODO ELSE
    
    uint32_t status_info = ((bytes_to_receive - 1) & 0x7FF) | ((port & 0x1U) << 24);
    // status_info = bswap32(status_info);
    address_space_write(&address_space_memory, status_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&status_info, 4);

    s->rxProduceIndex += 1;
    s->rxProduceIndex %= s->rxDescriptorNumber + 1;

    if (s->irqEnable & (1UL << 3) && control & (1UL << 31))
    {
        s->irqStatus |= 1UL << 3;
        qemu_irq_pulse(s->irq);
    }

    return size;
}

static ssize_t my_dual_enet_receive1(NetClientState *nc, const uint8_t *buf, size_t size)
{
    return my_dual_enet_receive(nc, buf, size, 0U);
}

static ssize_t my_dual_enet_receive2(NetClientState *nc, const uint8_t *buf, size_t size)
{
    return my_dual_enet_receive(nc, buf, size, 1U);
}

static int my_dual_enet_can_send(my_dual_enet_state *s)
{
    if (!s->txEnable || s->txConsumeIndex == s->txProduceIndex)
        return 0;
    return 1;
}

static void my_dual_enet_send(my_dual_enet_state *s)
{
    while (my_dual_enet_can_send(s))
    {
        uint32_t descriptor_address1 = s->txDescriptor + s->txConsumeIndex * 12;
        uint32_t descriptor_address2 = descriptor_address1 + 4;
        uint32_t control_address = descriptor_address2 + 4;
        uint32_t status_address = s->txStatus + s->txConsumeIndex * 4;

        // buffer_address = bswap32(buffer_address);
        uint32_t control;
        address_space_read(&address_space_memory, control_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&control, 4);
        // control = bswap32(control);
        size_t buffer_size1 = ((control >> 00) & 0x7FF) + 1;
        size_t buffer_size2 = ((control >> 12) & 0x7FF) + 1;
        size_t buffer_size = buffer_size1 + buffer_size2;

        uint32_t buffer_address1;
        uint32_t buffer_address2;
        address_space_read(&address_space_memory, descriptor_address1, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&buffer_address1, 4);
        address_space_read(&address_space_memory, descriptor_address2, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&buffer_address2, 4);

        
        // bytes_to_receive = bswap32(bytes_to_receive);
        size_t bytes_to_send = MIN(buffer_size, sizeof(s->txBuffer));
        size_t bytes_to_send1 = MIN(buffer_size1, sizeof(s->txBuffer));
        size_t bytes_to_send2 = bytes_to_send - bytes_to_send1;
        address_space_read(&address_space_memory, buffer_address1, MEMTXATTRS_UNSPECIFIED, (unsigned char *)s->txBuffer, bytes_to_send1);
        address_space_read(&address_space_memory, buffer_address2, MEMTXATTRS_UNSPECIFIED, (unsigned char *)s->txBuffer + bytes_to_send1, bytes_to_send2);

        unsigned int port = (control >> 24) & 0x1;
        qemu_send_packet(qemu_get_queue(s->nic[port]), (unsigned char *)s->txBuffer, bytes_to_send);

        uint32_t status_info = (bytes_to_send - 1) & 0x7FF;
        // status_info = bswap32(status_info);
        address_space_write(&address_space_memory, status_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&status_info, 4);

        s->txConsumeIndex += 1;
        s->txConsumeIndex %= s->txDescriptorNumber + 1;

        if (s->irqEnable & (1UL << 7) && control & (1UL << 31))
        {
            s->irqStatus |= 1UL << 7;
            qemu_irq_pulse(s->irq);
        }
    }
}

static uint64_t my_dual_enet_read(void *opaque, hwaddr offset, unsigned size)
{
    my_dual_enet_state *s = (my_dual_enet_state *)opaque;
    switch (offset) {
    case MY_DUAL_ENET_CONTROL_REGISTER_OFFSET:
        return ((uint32_t)(s->rxEnable) << 0) | ((uint32_t)(s->txEnable) << 1);

    case MY_DUAL_ENET_RX_DESCRIPTOR_REGISTER_OFFSET:
        return s->rxDescriptor;
    case MY_DUAL_ENET_RX_STATUS_REGISTER_OFFSET:
        return s->rxStatus;
    case MY_DUAL_ENET_RX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        return s->rxDescriptorNumber;
    case MY_DUAL_ENET_RX_PRODUCE_INDEX_REGISTER_OFFSET:
        return s->rxProduceIndex;
    case MY_DUAL_ENET_RX_CONSUME_INDEX_REGISTER_OFFSET:
        return s->rxConsumeIndex;

    case MY_DUAL_ENET_TX_DESCRIPTOR_REGISTER_OFFSET:
        return s->txDescriptor;
    case MY_DUAL_ENET_TX_STATUS_REGISTER_OFFSET:
        return s->txStatus;
    case MY_DUAL_ENET_TX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        return s->txDescriptorNumber;
    case MY_DUAL_ENET_TX_PRODUCE_INDEX_REGISTER_OFFSET:
        return s->txProduceIndex;
    case MY_DUAL_ENET_TX_CONSUME_INDEX_REGISTER_OFFSET:
        return s->txConsumeIndex;

    case MY_DUAL_ENET_INT_STATUS_REGISTER_OFFSET:
        return s->irqStatus;
    case MY_DUAL_ENET_INT_ENABLE_REGISTER_OFFSET:
        return s->irqEnable;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "my_dual_enet_rd%d: Illegal register 0x02%" HWADDR_PRIx "\n", size * 8, offset);
        return 0;
    }
}

static void my_dual_enet_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    my_dual_enet_state *s = (my_dual_enet_state *)opaque;
    switch (offset) {
    case MY_DUAL_ENET_COMMAND_REGISTER_OFFSET:
        if (true && (value & (1 << 0)))
        {
            s->rxConsumeIndex += 1;
            s->rxConsumeIndex %= s->rxDescriptorNumber + 1;
            for (unsigned int i = 0; i < NIC_PORTS; i++)
                if (my_dual_enet_can_receive(s->nic[i]->ncs))
                    qemu_flush_queued_packets(qemu_get_queue(s->nic[i]));
        }
        if (true && (value & (1 << 1)))
        {
            s->txProduceIndex += 1;
            s->txProduceIndex %= s->txDescriptorNumber + 1;
            my_dual_enet_send(s);
        }
        if (value & (1 << 3 | 1 << 5))
            my_dual_enet_reset_rx(s);
        if (value & (1 << 3 | 1 << 4))
            my_dual_enet_reset_tx(s);
        break;
    case MY_DUAL_ENET_CONTROL_REGISTER_OFFSET:
    {
        uint32_t rxEnable = s->rxEnable;
        s->rxEnable = value & (1 << 0);
        for (unsigned int i = 0; i < NIC_PORTS; i++)
            if (!rxEnable && my_dual_enet_can_receive(s->nic[i]->ncs))
                qemu_flush_queued_packets(qemu_get_queue(s->nic[i]));
        uint32_t txEnable = s->txEnable;
        s->txEnable = value & (1 << 1);
        if (!txEnable)
            my_dual_enet_send(s);
        break;
    }
    case MY_DUAL_ENET_RX_STATUS_REGISTER_OFFSET:
        s->rxStatus = value;
        break;
    case MY_DUAL_ENET_RX_DESCRIPTOR_REGISTER_OFFSET:
        s->rxDescriptor = value;
        break;
    case MY_DUAL_ENET_RX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        s->rxDescriptorNumber = value;
        break;

    case MY_DUAL_ENET_TX_STATUS_REGISTER_OFFSET:
        s->txStatus = value;
        break;
    case MY_DUAL_ENET_TX_DESCRIPTOR_REGISTER_OFFSET:
        s->txDescriptor = value;
        break;
    case MY_DUAL_ENET_TX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        s->txDescriptorNumber = value;
        break;

    case MY_DUAL_ENET_INT_ENABLE_REGISTER_OFFSET:
        s->irqEnable = value;
        break;
    case MY_DUAL_ENET_INT_CLEAR_REGISTER_OFFSET:
        s->irqStatus &= ~(value);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "my_dual_enet_wr%d: Illegal register 0x02%" HWADDR_PRIx " = 0x%" PRIx64 "\n", size * 8, offset, value);
    }
}

static const MemoryRegionOps my_dual_enet_ops = {
    .read = my_dual_enet_read,
    .write = my_dual_enet_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void my_dual_enet_reset(DeviceState *dev)
{
    my_dual_enet_state *s =  MY_DUAL_ENET(dev);
    my_dual_enet_reset_rx(s);
    my_dual_enet_reset_tx(s);
    qemu_set_irq(s->irq, 0);
}

static NetClientInfo net_my_dual_enet_info[NIC_PORTS] = {
    {
        .type = NET_CLIENT_DRIVER_NIC,
        .size = sizeof(NICState),
        .receive = my_dual_enet_receive1,
        .can_receive = my_dual_enet_can_receive,
    },
    {
        .type = NET_CLIENT_DRIVER_NIC,
        .size = sizeof(NICState),
        .receive = my_dual_enet_receive2,
        .can_receive = my_dual_enet_can_receive,
    }
};

static void my_dual_enet_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    my_dual_enet_state *s = MY_DUAL_ENET(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &my_dual_enet_ops, s, "my_dual_enet", 0x4000);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);

    for (unsigned int i = 0; i < NIC_PORTS; i++)
    {
        qemu_macaddr_default_if_unset(&s->conf[i].macaddr);
        s->nic[i] = qemu_new_nic(&net_my_dual_enet_info[i], &s->conf[i], object_get_typename(OBJECT(dev)), dev->id, s);
        qemu_format_nic_info_str(qemu_get_queue(s->nic[i]), s->conf[i].macaddr.a);
    }
}

static Property my_dual_enet_properties[] = {
    DEFINE_NIC_PROPERTIES(my_dual_enet_state, conf[0]),
    DEFINE_NIC_PROPERTIES(my_dual_enet_state, conf[1]),
    DEFINE_PROP_END_OF_LIST(),
};

static void my_dual_enet_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = my_dual_enet_realize;
    dc->reset = my_dual_enet_reset;
    dc->props = my_dual_enet_properties;
}

static const TypeInfo my_dual_enet_info = {
    .name          = TYPE_MY_DUAL_ENET,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(my_dual_enet_state),
    .class_init    = my_dual_enet_class_init,
};

static void my_dual_enet_register_types(void)
{
    type_register_static(&my_dual_enet_info);
}

type_init(my_dual_enet_register_types)
