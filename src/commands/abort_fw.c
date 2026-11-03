#include "cxl_fw.h"

int abort_fw_transfer_execute(void)
{
    char confirmation[32] = {0};

    fprintf(stderr,
            "WARNING: this sends the CXL TRANSFER_FW ABORT action to %s.\n"
            "It clears QEMU's in-progress transfer state if one exists.\n"
            "It does not send firmware data and does not activate a slot.\n"
            "Type ABORT in uppercase to continue: ", CXL_DEVICE);
    if (fgets(confirmation, sizeof(confirmation), stdin) == NULL ||
        strcmp(confirmation, "ABORT\n") != 0) {
        fprintf(stderr, "Confirmation not received; no ABORT command was sent.\n");
        return -1;
    }

    int fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open " CXL_DEVICE);
        return -1;
    }

    struct fw_transfer_header request = {0};
    request.action = FW_ACTION_ABORT;
    /* QEMU handles ABORT before validating the slot or offset fields. */
    request.slot = 2;
    request.offset_le = htole32(0);

    uint32_t mailbox_return = UINT32_MAX;
    int out_size = send_raw_command(fd, CXL_FW_TRANSFER_OPCODE,
                                    (uint8_t *)&request, sizeof(request),
                                    NULL, 0, &mailbox_return);
    int rc = 0;
    if (out_size < 0) {
        rc = -1;
    } else if (mailbox_return != CXL_MBOX_SUCCESS) {
        fprintf(stderr,
                "TRANSFER_FW ABORT failed with mailbox return code 0x%x.\n",
                mailbox_return);
        rc = -1;
    } else {
        puts("TRANSFER_FW ABORT accepted by the device.");
        puts("No firmware data was sent; no firmware was activated.");
    }

    if (close(fd) != 0) {
        perror("close CXL device");
        rc = -1;
    }
    return rc;
}