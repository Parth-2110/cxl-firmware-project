#ifndef CXL_FW_H
#define CXL_FW_H

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <endian.h>
#include <linux/cxl_mem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#define CXL_DEVICE "/dev/cxl/mem0"
#define FW_INFO_SIZE 0x50U
#define FW_TRANSFER_HEADER_SIZE 128U
#define FW_TRANSFER_ALIGNMENT 128U
#define DRY_RUN_MAX_IMAGE_SIZE (32U * 1024U * 1024U)
#define FW_TRANSFER_MAX_IMAGE_SIZE (32U * 1024U * 1024U)
#define CXL_FW_TRANSFER_OPCODE 0x0201U
#define CXL_MBOX_SUCCESS 0x00U

enum fw_transfer_action {
    FW_ACTION_FULL = 0,
    FW_ACTION_INITIATE = 1,
    FW_ACTION_CONTINUE = 2,
    FW_ACTION_END = 3,
    FW_ACTION_ABORT = 4
};

struct fw_transfer_header {
    uint8_t action;
    uint8_t slot;
    uint8_t reserved[2];
    uint32_t offset_le;
    uint8_t reserved2[0x78];
} __attribute__((packed));

_Static_assert(sizeof(struct fw_transfer_header) ==
               FW_TRANSFER_HEADER_SIZE,
               "Firmware-transfer header must be 128 bytes");
_Static_assert(offsetof(struct fw_transfer_header, offset_le) == 4,
               "Firmware-transfer offset must start at byte 4");

struct fw_info {
    uint8_t num_slots;
    uint8_t slot_info;
    uint8_t activation_cap;
    uint8_t reserved[13];
    char slot_revision[4][16];
} __attribute__((packed));

_Static_assert(sizeof(struct fw_info) == FW_INFO_SIZE,
               "Firmware information structure must be 80 bytes");

int parse_uint(const char *text, unsigned int *value);
int build_fw_transfer_header(uint8_t *buffer, size_t buffer_size,
                             enum fw_transfer_action action,
                             unsigned int slot, uint32_t offset);
int build_fw_transfer_request(uint8_t *request, size_t request_capacity,
                              size_t *request_size,
                              enum fw_transfer_action action,
                              unsigned int slot, uint32_t offset,
                              const uint8_t *data, size_t data_size);
int test_transfer_header(void);
int test_transfer_request(void);
int query_commands(int fd);
int get_fw_info(int fd);
int transfer_fw_dry_run(int argc, char **argv);
int send_raw_command(int fd, uint16_t opcode,
                     uint8_t *input, size_t input_size,
                     uint8_t *output, size_t output_capacity,
                     uint32_t *mailbox_return);
int read_fw_info_for_transfer(int fd, struct fw_info *info);
int read_payload_max(unsigned int *payload_max);
int abort_fw_transfer_execute(void);
int transfer_fw_execute(int argc, char **argv);
int fault_inject_execute(int argc, char **argv);
void usage(const char *program);

#endif