#include "cxl_fw.h"

static void print_revision(unsigned int slot, const char revision[16])
{
    size_t length = 16;

    while (length > 0 &&
           (revision[length - 1] == '\0' ||
            revision[length - 1] == ' ')) {
        length--;
    }

    printf("  Slot %u revision: ", slot);

    if (length == 0) {
        puts("(empty)");
        return;
    }

    printf("%.*s\n", (int)length, revision);
}

int get_fw_info(int fd)
{
    struct fw_info info = {0};

    struct cxl_send_command cmd = {
        .id = CXL_MEM_COMMAND_ID_GET_FW_INFO,
        .flags = 0,
        .retval = 0,
        .in = {
            .size = 0,
            .rsvd = 0,
            .payload = 0,
        },
        .out = {
            .size = sizeof(info),
            .rsvd = 0,
            .payload = (uint64_t)(uintptr_t)&info,
        },
    };

    if (ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd) < 0) {
        perror("CXL_MEM_SEND_COMMAND (GET_FW_INFO)");
        return -1;
    }

    puts("GET_FW_INFO ioctl completed.");
    printf("Device mailbox return value: 0x%08x\n", cmd.retval);
    printf("Response size: %u bytes\n", cmd.out.size);

    if (cmd.retval != 0) {
        fprintf(stderr,
                "Device reported a nonzero mailbox return value.\n");
        return -1;
    }

    if (cmd.out.size != sizeof(info)) {
        fprintf(stderr,
                "Unexpected response size: expected %zu bytes.\n",
                sizeof(info));
        return -1;
    }

    unsigned int current_slot = info.slot_info & 0x07;
    unsigned int next_slot = (info.slot_info >> 3) & 0x07;

    puts("\nFirmware information");
    printf("  Number of slots: %u\n", info.num_slots);
    printf("  Current slot: %u\n", current_slot);
    printf("  Next slot: %u\n", next_slot);
    printf("  Live activation supported: %s\n",
           (info.activation_cap & 0x01) ? "yes" : "no");

    for (unsigned int i = 0; i < 4; i++)
        print_revision(i + 1, info.slot_revision[i]);

    return 0;
}

int read_fw_info_for_transfer(int fd, struct fw_info *info)
{
    struct cxl_send_command cmd = {0};
    memset(info, 0, sizeof(*info));

    cmd.id = CXL_MEM_COMMAND_ID_GET_FW_INFO;
    cmd.in.size = 0;
    cmd.in.payload = 0;
    cmd.out.size = sizeof(*info);
    cmd.out.payload = (uint64_t)(uintptr_t)info;

    if (ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd) < 0) {
        perror("CXL_MEM_SEND_COMMAND (GET_FW_INFO before transfer)");
        return -1;
    }
    if (cmd.retval != CXL_MBOX_SUCCESS || cmd.out.size != sizeof(*info)) {
        fprintf(stderr,
                "GET_FW_INFO pre-transfer check failed (retval=0x%x, size=%u).\n",
                cmd.retval, cmd.out.size);
        return -1;
    }
    return 0;
}