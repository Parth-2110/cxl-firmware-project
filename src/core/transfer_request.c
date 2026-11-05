#include "cxl_fw.h"

int build_fw_transfer_header(uint8_t *buffer, size_t capacity,
                             enum fw_transfer_action action,
                             unsigned int slot, uint32_t offset)
{
    if (buffer == NULL || capacity < FW_TRANSFER_HEADER_SIZE ||
        action < FW_ACTION_FULL || action > FW_ACTION_ABORT ||
        slot > UINT8_MAX || (action != FW_ACTION_ABORT && slot == 0)) {
        fprintf(stderr, "Invalid firmware-transfer header arguments.\n");
        return -1;
    }

    struct fw_transfer_header header = {0};
    header.action = (uint8_t)action;
    header.slot = (uint8_t)slot;
    header.offset_le = htole32(offset);
    memset(buffer, 0, FW_TRANSFER_HEADER_SIZE);
    memcpy(buffer, &header, sizeof(header));
    return 0;
}

int build_fw_transfer_request(uint8_t *out, size_t capacity, size_t *out_size,
                              enum fw_transfer_action action,
                              unsigned int slot, uint32_t offset,
                              const uint8_t *data, size_t data_size)
{
    if (out == NULL || out_size == NULL || (data_size > 0 && data == NULL) ||
        data_size > SIZE_MAX - FW_TRANSFER_HEADER_SIZE) {
        fprintf(stderr, "Invalid transfer request arguments.\n");
        return -1;
    }

    size_t total_size = FW_TRANSFER_HEADER_SIZE + data_size;
    if (total_size > capacity) {
        fprintf(stderr, "Transfer request needs %zu bytes, capacity is %zu.\n", total_size, capacity);
        return -1;
    }
    if (build_fw_transfer_header(out, capacity, action, slot, offset) != 0)
        return -1;
    if (data_size > 0)
        memcpy(out + FW_TRANSFER_HEADER_SIZE, data, data_size);
    *out_size = total_size;
    return 0;
}