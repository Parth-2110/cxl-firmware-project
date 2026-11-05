#include "cxl_fw.h"

int send_raw_command(int fd, uint16_t opcode,
                     uint8_t *input, size_t input_size,
                     uint8_t *output, size_t output_capacity,
                     uint32_t *mailbox_return)
{
    if (input_size > UINT32_MAX || output_capacity > UINT32_MAX) {
        fprintf(stderr, "Raw command payload is too large for the ioctl ABI.\n");
        return -1;
    }

    struct cxl_send_command cmd = {0};
    cmd.id = CXL_MEM_COMMAND_ID_RAW;
    cmd.raw.opcode = opcode;
    cmd.in.size = (uint32_t)input_size;
    cmd.in.payload = input_size ? (uint64_t)(uintptr_t)input : 0;
    cmd.out.size = (uint32_t)output_capacity;
    cmd.out.payload = output_capacity ? (uint64_t)(uintptr_t)output : 0;

    if (ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd) < 0) {
        fprintf(stderr, "Raw CXL opcode 0x%04x ioctl failed: ", opcode);
        perror(NULL);
        return -1;
    }
    if (mailbox_return != NULL)
        *mailbox_return = cmd.retval;
    if (cmd.out.size > output_capacity) {
        fprintf(stderr, "Raw opcode 0x%04x returned too much output data.\n", opcode);
        return -1;
    }
    return (int)cmd.out.size;
}