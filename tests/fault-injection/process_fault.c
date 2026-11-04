
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/cxl_mem.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CXL_DEVICE "/dev/cxl/mem0"
#define RAW_COMMAND_ID 2
#define FW_TRANSFER_OPCODE 0x0201
#define BG_STARTED 0x0001
#define FW_HEADER_SIZE 128U
#define DATA_SIZE 16U

int main(void)
{
    int fd;
    int rc;

    uint8_t payload[FW_HEADER_SIZE + DATA_SIZE] = {0};
    uint8_t response[256] = {0};

    struct cxl_send_command cmd = {0};

    /* Construct a FULL firmware transfer to slot 2. */
    payload[0] = 0x00;
    payload[1] = 2;

    for (size_t i = FW_HEADER_SIZE;
         i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i - FW_HEADER_SIZE);
    }

    printf("Process-level fault injection\n");
    printf("------------------------------\n");
    printf("Device: %s\n", CXL_DEVICE);
    printf("Opcode: 0x%04X\n", FW_TRANSFER_OPCODE);
    printf("Action: FULL\n");
    printf("Target slot: 2\n");
    printf("Payload size: %zu bytes\n", sizeof(payload));

    fd = open(CXL_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open CXL device");
        return 1;
    }

    cmd.id = RAW_COMMAND_ID;
    cmd.raw.opcode = FW_TRANSFER_OPCODE;

    cmd.in.size = sizeof(payload);
    cmd.in.payload = (uint64_t)(uintptr_t)payload;

    cmd.out.size = sizeof(response);
    cmd.out.payload = (uint64_t)(uintptr_t)response;

    printf("\nSubmitting firmware transfer...\n");
    fflush(stdout);

    rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);

    if (rc < 0) {
        perror("CXL_MEM_SEND_COMMAND");
        close(fd);
        return 1;
    }

    printf("Ioctl completed successfully.\n");
    printf("Mailbox return code: 0x%04X\n",
           (unsigned int)cmd.retval);

    close(fd);

    if (cmd.retval == BG_STARTED) {
        printf("\nBACKGROUND OPERATION STARTED!\n");
        printf("The utility will NOT poll for completion.\n");
        printf("QEMU background operation should still be running.\n");
        return 0;
    }

    printf("\nUnexpected mailbox response.\n");
    printf("Expected BG_STARTED (0x%04X).\n", BG_STARTED);

    return 2;
}
