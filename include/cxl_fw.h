#ifndef CXL_FW_H
#define CXL_FW_H
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <linux/cxl_mem.h>
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
#define DEFAULT_FAULT_LOG_PATH "logs/protocol-fault-injection.csv"

enum fw_transfer_action { FW_ACTION_FULL=0, FW_ACTION_INITIATE=1, FW_ACTION_CONTINUE=2, FW_ACTION_END=3, FW_ACTION_ABORT=4 };
struct fw_transfer_header { uint8_t action, slot, reserved[2]; uint32_t offset_le; uint8_t reserved2[0x78]; } __attribute__((packed));
_Static_assert(sizeof(struct fw_transfer_header)==FW_TRANSFER_HEADER_SIZE,"Firmware-transfer header must be 128 bytes");
_Static_assert(offsetof(struct fw_transfer_header,offset_le)==4,"Firmware-transfer offset must start at byte 4");
struct fw_info { uint8_t num_slots,slot_info,activation_cap,reserved[13]; char slot_revision[4][16]; } __attribute__((packed));
_Static_assert(sizeof(struct fw_info)==FW_INFO_SIZE,"Firmware information structure must be 80 bytes");

struct fw_transfer_request { uint8_t *data; size_t size; size_t capacity; };
typedef int (*fw_request_hook)(uint8_t *request,size_t *request_size,size_t capacity,uint64_t part_index,void *context);
typedef int (*fw_submit_fn)(void *context,const uint8_t *request,size_t request_size,uint32_t *mailbox_return);
struct fw_transfer_engine { unsigned int slot; unsigned int payload_capacity; fw_submit_fn submit; void *submit_context; fw_request_hook hook; void *hook_context; int require_success; };

int parse_uint(const char*,unsigned int*);
int build_fw_transfer_header(uint8_t*,size_t,enum fw_transfer_action,unsigned int,uint32_t);
int build_fw_transfer_request(uint8_t*,size_t,size_t*,enum fw_transfer_action,unsigned int,uint32_t,const uint8_t*,size_t);
int test_transfer_header(void); int test_transfer_request(void);
int query_commands(int); int get_fw_info(int); int read_fw_info_for_transfer(int,struct fw_info*); int read_payload_max(unsigned int*);
int send_raw_command(int,uint16_t,uint8_t*,size_t,uint8_t*,size_t,uint32_t*);
int fw_transfer_submit_ioctl(void*,const uint8_t*,size_t,uint32_t*);
int fw_transfer_run_image(struct fw_transfer_engine*,FILE*,uint64_t);
int fw_transfer_run_prepared(struct fw_transfer_engine*,uint8_t*,size_t,uint64_t,uint32_t*);
int transfer_fw_dry_run(int,char**); int transfer_fw_execute(int,char**); int abort_fw_transfer_execute(void); int fault_inject_execute(int,char**);
void usage(const char*);
#endif