#include "cxl_fw.h"

int build_fw_transfer_header(
    uint8_t *buffer,
    size_t buffer_size,
    enum fw_transfer_action action,
    unsigned int slot,
    uint32_t offset_units)
{
    struct fw_transfer_header header = {0};

    if (buffer == NULL || buffer_size < FW_TRANSFER_HEADER_SIZE) {
        fprintf(stderr, "Transfer header buffer is too small.\n");
        return -1;
    }

    if (action < FW_ACTION_FULL || action > FW_ACTION_ABORT) {
        fprintf(stderr, "Invalid firmware-transfer action.\n");
        return -1;
    }

    if (slot > UINT8_MAX) {
        fprintf(stderr, "Slot value exceeds the header field size.\n");
        return -1;
    }

    if (action != FW_ACTION_ABORT && slot == 0) {
        fprintf(stderr, "Target slot must be nonzero.\n");
        return -1;
    }

    header.action = (uint8_t)action;
    header.slot = (uint8_t)slot;
    header.offset_le = htole32(offset_units);

    memset(buffer, 0, FW_TRANSFER_HEADER_SIZE);
    memcpy(buffer, &header, sizeof(header));

    return 0;
}

int build_fw_transfer_request(
    uint8_t *out,
    size_t out_capacity,
    size_t *out_size,
    enum fw_transfer_action action,
    unsigned int slot,
    uint32_t offset_units,
    const uint8_t *data,
    size_t data_size)
{
    size_t total_size;

    if (out == NULL || out_size == NULL) {
        fprintf(stderr, "Invalid request-buffer argument.\n");
        return -1;
    }

    if (data_size > 0 && data == NULL) {
        fprintf(stderr, "Data pointer is NULL for a nonempty chunk.\n");
        return -1;
    }

    if (data_size > SIZE_MAX - FW_TRANSFER_HEADER_SIZE) {
        fprintf(stderr, "Transfer request size overflow.\n");
        return -1;
    }

    total_size = FW_TRANSFER_HEADER_SIZE + data_size;

    if (total_size > out_capacity) {
        fprintf(stderr,
                "Transfer request needs %zu bytes, but capacity is %zu.\n",
                total_size, out_capacity);
        return -1;
    }

    if (build_fw_transfer_header(out, out_capacity, action,
                                 slot, offset_units) != 0)
        return -1;

    if (data_size > 0)
        memcpy(out + FW_TRANSFER_HEADER_SIZE, data, data_size);

    *out_size = total_size;
    return 0;
}