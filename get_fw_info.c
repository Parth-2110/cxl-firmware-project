// Retrieves and displays CXL firmware slot information, activation capabilities, and firmware revisions.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <linux/cxl_mem.h>

/* Get Firmware Info response: 80 bytes */
struct fw_info_response {
    uint8_t num_slots;
    uint8_t slot_info;
    uint8_t activation_cap;
    uint8_t reserved[13];
    char slot_1_revision[16];
    char slot_2_revision[16];
    char slot_3_revision[16];
    char slot_4_revision[16];
} __attribute__((packed));

int main(void)
{
    int fd;
    int rc;
    struct cxl_send_command cmd;
    struct fw_info_response response = {0};

    /* Open the CXL memory device */
    fd = open("/dev/cxl/mem0", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/cxl/mem0");
        return EXIT_FAILURE;
    }

    /* Prepare the Get Firmware Info command */
    memset(&cmd, 0, sizeof(cmd));
    cmd.id = 4;  /* CXL Get Firmware Info */
    cmd.out.size = sizeof(response);
    cmd.out.payload = (uintptr_t)&response;

    printf("Sending GET_FW_INFO command...\n");

    /* Send the command to the kernel driver */
    rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);
    if (rc < 0) {
        perror("CXL_MEM_SEND_COMMAND failed");
        close(fd);
        return EXIT_FAILURE;
    }

       printf("Command completed at ioctl level.\n");
    printf("Device return code: %u\n", cmd.retval);
    printf("Response size: %u bytes\n\n", cmd.out.size);

    if (cmd.retval != 0) {
        fprintf(stderr, "Device reported a command error.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (cmd.out.size != sizeof(response)) {
        fprintf(stderr, "Unexpected response size.\n");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Firmware Information\n");
    printf("--------------------\n");
    printf("Number of firmware slots: %u\n", response.num_slots);

    /* Bits 0-2 contain the current slot */
    printf("Current firmware slot: %u\n",
           response.slot_info & 0x07);

    /* Bits 3-5 contain the next slot */
    printf("Next firmware slot: %u\n",
           (response.slot_info >> 3) & 0x07);

    printf("Live activation supported: %s\n",
           (response.activation_cap & 0x01) ? "Yes" : "No");

    printf("\nFirmware revisions:\n");
    printf("Slot 1: %.16s\n", response.slot_1_revision);
    printf("Slot 2: %.16s\n", response.slot_2_revision);
    printf("Slot 3: %.16s\n", response.slot_3_revision);
    printf("Slot 4: %.16s\n", response.slot_4_revision);

    close(fd);
    return EXIT_SUCCESS;
}
