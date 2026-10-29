#include <errno.h>
#include <endian.h>
#include <fcntl.h>
#include <linux/cxl_mem.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(void)
{
    struct {
        uint8_t status;
        uint8_t reserved;
        uint16_t opcode;
        uint16_t returncode;
        uint16_t vendor_ext_status;
    } __attribute__((packed)) result = {0};

    struct cxl_send_command cmd = {0};

    int fd = open("/dev/cxl/mem0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open /dev/cxl/mem0");
        return 1;
    }

    cmd.id = 2;
    cmd.raw.opcode = 0x0002;
    cmd.out.size = sizeof(result);
    cmd.out.payload = (uint64_t)(uintptr_t)&result;

    printf("Querying BACKGROUND_OPERATION_STATUS (0x0002)...\n");

    if (ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd) < 0) {
        perror("CXL_MEM_SEND_COMMAND");
        close(fd);
        return 1;
    }

    close(fd);

    printf("Ioctl result: success\n");
    printf("Mailbox return code: 0x%08X\n", cmd.retval);
    printf("Response size: %u bytes\n", cmd.out.size);

    if (cmd.retval != 0 || cmd.out.size != sizeof(result)) {
        fprintf(stderr, "FAIL: unexpected mailbox result or response size\n");
        return 1;
    }

    printf("Raw status byte: 0x%02X\n", result.status);
    printf("Completion: %u%%\n", result.status >> 1);
    printf("Running: %s\n", (result.status & 1) ? "YES" : "NO");
    printf("Reported opcode: 0x%04X\n", le16toh(result.opcode));
    printf("Operation return code: 0x%04X\n",
           le16toh(result.returncode));
    printf("Vendor extended status: 0x%04X\n",
           le16toh(result.vendor_ext_status));

    return 0;
}
