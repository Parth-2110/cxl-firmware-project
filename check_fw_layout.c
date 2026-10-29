// Displays CXL firmware transfer and activation structure sizes and field offsets.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct cxl_mbox_transfer_fw {
    uint8_t action;
    uint8_t slot;
    uint8_t reserved[2];
    uint32_t offset;
    uint8_t reserved2[0x78];
    uint8_t data[];
} __attribute__((packed));

struct cxl_mbox_activate_fw {
    uint8_t action;
    uint8_t slot;
} __attribute__((packed));

int main(void)
{
    printf("Firmware transfer header size: %zu bytes\n",
           offsetof(struct cxl_mbox_transfer_fw, data));

    printf("Firmware data begins at offset: %zu bytes\n",
           offsetof(struct cxl_mbox_transfer_fw, data));

    printf("Transfer offset field position: %zu bytes\n",
           offsetof(struct cxl_mbox_transfer_fw, offset));

    printf("Activation structure size: %zu bytes\n",
           sizeof(struct cxl_mbox_activate_fw));

    return 0;
}
