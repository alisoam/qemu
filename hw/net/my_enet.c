#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "net/net.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include <zlib.h>

//#define DEBUG_MY_ENET 1

#ifdef DEBUG_MY_ENET
#define DPRINTF(fmt, ...) \
do { printf("my_enet: " fmt , ## __VA_ARGS__); } while (0)
#define BADF(fmt, ...) \
do { fprintf(stderr, "mt_enet: error: " fmt , ## __VA_ARGS__); exit(1);} while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#define BADF(fmt, ...) \
do { fprintf(stderr, "my_enet: error: " fmt , ## __VA_ARGS__);} while (0)
#endif

#define MY_ENET_MAC1_REGISTER_OFFSET                    0x000
#define MY_ENET_MAC2_REGISTER_OFFSET                    0x004

#define MY_ENET_COMMAND_REGISTER_OFFSET                 0x100
#define MY_ENET_CONTROL_REGISTER_OFFSET                 0x104
#define MY_ENET_RX_DESCRIPTOR_REGISTER_OFFSET           0x108
#define MY_ENET_RX_STATUS_REGISTER_OFFSET               0x10C
#define MY_ENET_RX_DESCRIPTOR_NUMBER_REGISTER_OFFSET    0x110
#define MY_ENET_RX_PRODUCE_INDEX_REGISTER_OFFSET        0x114
#define MY_ENET_RX_CONSUME_INDEX_REGISTER_OFFSET        0x118
#define MY_ENET_TX_DESCRIPTOR_REGISTER_OFFSET           0x11C
#define MY_ENET_TX_STATUS_REGISTER_OFFSET               0x120
#define MY_ENET_TX_DESCRIPTOR_NUMBER_REGISTER_OFFSET    0x124
#define MY_ENET_TX_PRODUCE_INDEX_REGISTER_OFFSET        0x128
#define MY_ENET_TX_CONSUME_INDEX_REGISTER_OFFSET        0x12C

#define MY_ENET_INT_STATUS_REGISTER_OFFSET              0xFE0
#define MY_ENET_INT_ENABLE_REGISTER_OFFSET              0xFE4
#define MY_ENET_INT_CLEAR_REGISTER_OFFSET               0xFE8


#define TYPE_MY_ENET "my_enet"
#define MY_ENET(obj) \
    OBJECT_CHECK(my_enet_state, (obj), TYPE_MY_ENET)

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

    NICState *nic;
    NICConf conf;
    qemu_irq irq;
    MemoryRegion mmio;
} my_enet_state;

void DumpHex(const void* data, size_t size);
void DumpHex(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~')
            ascii[i % 16] = ((unsigned char*)data)[i];
        else
            ascii[i % 16] = '.';
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8)
                    printf(" ");
                for (j = (i+1) % 16; j < 16; ++j)
                    printf("   ");
                printf("|  %s \n", ascii);
            }
        }
    }
}

static void my_enet_reset_rx(my_enet_state *s)
{
    s->rxEnable = false;
    s->rxDescriptorNumber = 0;
    s->rxStatus = 0;
    s->rxProduceIndex = 0;
    s->rxConsumeIndex = 0;
    s->irqStatus &= ~(1UL << 2 | 1UL << 3);
}

static void my_enet_reset_tx(my_enet_state *s)
{
    s->txEnable = false;
    s->txDescriptorNumber = 0;
    s->txStatus = 0;
    s->txProduceIndex = 0;
    s->txConsumeIndex = 0;
    s->irqStatus &= ~(1UL << 6 | 1UL << 7);
}

static int my_enet_can_receive(NetClientState *nc)
{
    my_enet_state *s = qemu_get_nic_opaque(nc);

    if (!s->rxEnable ||
        s->rxProduceIndex == (s->rxConsumeIndex + s->rxDescriptorNumber) % (s->rxDescriptorNumber + 1))
        return 0;
    return 1;
}

static ssize_t my_enet_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    if (size == 0)
        return 0;

    uint32_t crc = crc32(~0, buf, size);
    my_enet_state *s = qemu_get_nic_opaque(nc);

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
    size_t bytes_to_receive = size + 4;
    // bytes_to_receive = bswap32(bytes_to_receive);
    if (buffer_size >= bytes_to_receive)
    {
        address_space_write(&address_space_memory, buffer_address, MEMTXATTRS_UNSPECIFIED, buf, size);
        address_space_write(&address_space_memory, buffer_address + size, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&crc, 4);
    }
    // TODO ELSE
    
    uint32_t status_info = (bytes_to_receive - 1) & 0x7FF;
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

static int my_enet_can_send(NetClientState *nc)
{
    my_enet_state *s = qemu_get_nic_opaque(nc);

    if (!s->txEnable || s->txConsumeIndex == s->txProduceIndex)
        return 0;
    return 1;
}

static void my_enet_send(NetClientState *nc)
{
    my_enet_state *s = qemu_get_nic_opaque(nc);

    while (my_enet_can_send(nc))
    {
        uint32_t descriptor_address = s->txDescriptor + s->txConsumeIndex * 8;
        uint32_t control_address = descriptor_address + 4;
        uint32_t status_address = s->txStatus + s->txConsumeIndex * 4;

        uint32_t buffer_address;
        address_space_read(&address_space_memory, descriptor_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&buffer_address, 4);
        // buffer_address = bswap32(buffer_address);
        uint32_t control;
        address_space_read(&address_space_memory, control_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)&control, 4);
        // control = bswap32(control);

        size_t buffer_size = (control & 0x7FF) + 1;
        // bytes_to_receive = bswap32(bytes_to_receive);
        size_t bytes_to_send = MIN(buffer_size, sizeof(s->txBuffer));
        address_space_read(&address_space_memory, buffer_address, MEMTXATTRS_UNSPECIFIED, (unsigned char *)s->txBuffer, bytes_to_send);

        qemu_send_packet(qemu_get_queue(s->nic), (unsigned char *)s->txBuffer, bytes_to_send);

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

static uint64_t my_enet_read(void *opaque, hwaddr offset, unsigned size)
{
    my_enet_state *s = (my_enet_state *)opaque;
    switch (offset) {
    case MY_ENET_CONTROL_REGISTER_OFFSET:
        return ((uint32_t)(s->rxEnable) << 0) | ((uint32_t)(s->txEnable) << 1);

    case MY_ENET_RX_DESCRIPTOR_REGISTER_OFFSET:
        return s->rxDescriptor;
    case MY_ENET_RX_STATUS_REGISTER_OFFSET:
        return s->rxStatus;
    case MY_ENET_RX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        return s->rxDescriptorNumber;
    case MY_ENET_RX_PRODUCE_INDEX_REGISTER_OFFSET:
        return s->rxProduceIndex;
    case MY_ENET_RX_CONSUME_INDEX_REGISTER_OFFSET:
        return s->rxConsumeIndex;

    case MY_ENET_TX_DESCRIPTOR_REGISTER_OFFSET:
        return s->txDescriptor;
    case MY_ENET_TX_STATUS_REGISTER_OFFSET:
        return s->txStatus;
    case MY_ENET_TX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        return s->txDescriptorNumber;
    case MY_ENET_TX_PRODUCE_INDEX_REGISTER_OFFSET:
        return s->txProduceIndex;
    case MY_ENET_TX_CONSUME_INDEX_REGISTER_OFFSET:
        return s->txConsumeIndex;

    case MY_ENET_INT_STATUS_REGISTER_OFFSET:
        return s->irqStatus;
    case MY_ENET_INT_ENABLE_REGISTER_OFFSET:
        return s->irqEnable;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "my_enet_rd%d: Illegal register 0x02%" HWADDR_PRIx "\n", size * 8, offset);
        return 0;
    }
}

static void my_enet_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    my_enet_state *s = (my_enet_state *)opaque;
    switch (offset) {
    case MY_ENET_COMMAND_REGISTER_OFFSET:
        if (true && (value & (1 << 0)))
        {
            s->rxConsumeIndex += 1;
            s->rxConsumeIndex %= s->rxDescriptorNumber + 1;
            if (my_enet_can_receive(s->nic->ncs))
                qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        if (true && (value & (1 << 1)))
        {
            s->txProduceIndex += 1;
            s->txProduceIndex %= s->txDescriptorNumber + 1;
            my_enet_send(s->nic->ncs);
        }
        if (value & (1 << 3 | 1 << 5))
            my_enet_reset_rx(s);
        if (value & (1 << 3 | 1 << 4))
            my_enet_reset_tx(s);
        break;
    case MY_ENET_CONTROL_REGISTER_OFFSET:
    {
        uint32_t rxEnable = s->rxEnable;
        s->rxEnable = value & (1 << 0);
        if (!rxEnable && my_enet_can_receive(s->nic->ncs))
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        uint32_t txEnable = s->txEnable;
        s->txEnable = value & (1 << 1);
        if (!txEnable)
            my_enet_send(s->nic->ncs);
        break;
    }
    case MY_ENET_RX_STATUS_REGISTER_OFFSET:
        s->rxStatus = value;
        break;
    case MY_ENET_RX_DESCRIPTOR_REGISTER_OFFSET:
        s->rxDescriptor = value;
        break;
    case MY_ENET_RX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        s->rxDescriptorNumber = value;
        break;

    case MY_ENET_TX_STATUS_REGISTER_OFFSET:
        s->txStatus = value;
        break;
    case MY_ENET_TX_DESCRIPTOR_REGISTER_OFFSET:
        s->txDescriptor = value;
        break;
    case MY_ENET_TX_DESCRIPTOR_NUMBER_REGISTER_OFFSET:
        s->txDescriptorNumber = value;
        break;

    case MY_ENET_INT_ENABLE_REGISTER_OFFSET:
        s->irqEnable = value;
        break;
    case MY_ENET_INT_CLEAR_REGISTER_OFFSET:
        s->irqStatus &= ~(value);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "my_enet_wr%d: Illegal register 0x02%" HWADDR_PRIx " = 0x%" PRIx64 "\n", size * 8, offset, value);
    }
}

static const MemoryRegionOps my_enet_ops = {
    .read = my_enet_read,
    .write = my_enet_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void my_enet_reset(DeviceState *dev)
{
    my_enet_state *s =  MY_ENET(dev);
    my_enet_reset_rx(s);
    my_enet_reset_tx(s);
    qemu_set_irq(s->irq, 0);
}

static NetClientInfo net_my_enet_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .receive = my_enet_receive,
    .can_receive = my_enet_can_receive,
};

static void my_enet_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    my_enet_state *s = MY_ENET(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &my_enet_ops, s, "my_enet", 0x4000);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);
    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_my_enet_info, &s->conf, object_get_typename(OBJECT(dev)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static Property my_enet_properties[] = {
    DEFINE_NIC_PROPERTIES(my_enet_state, conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void my_enet_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = my_enet_realize;
    dc->reset = my_enet_reset;
    dc->props = my_enet_properties;
}

static const TypeInfo my_enet_info = {
    .name          = TYPE_MY_ENET,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(my_enet_state),
    .class_init    = my_enet_class_init,
};

static void my_enet_register_types(void)
{
    type_register_static(&my_enet_info);
}

type_init(my_enet_register_types)
