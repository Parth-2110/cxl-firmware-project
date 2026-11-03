#include "cxl_fw.h"

int query_commands(int fd)
{
    struct cxl_mem_query_commands query = {0};

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, &query) < 0) {
        perror("CXL_MEM_QUERY_COMMANDS (count)");
        return -1;
    }

    uint32_t count = query.n_commands;

    size_t bytes = sizeof(query) +
                   (size_t)count * sizeof(struct cxl_command_info);

    struct cxl_mem_query_commands *result = calloc(1, bytes);

    if (result == NULL) {
        perror("calloc");
        return -1;
    }

    result->n_commands = count;

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, result) < 0) {
        perror("CXL_MEM_QUERY_COMMANDS (list)");
        free(result);
        return -1;
    }

    uint32_t empty_count = 0;
    puts("Kernel command table entries:");

    for (uint32_t i = 0; i < result->n_commands; i++) {
        const struct cxl_command_info *info = &result->commands[i];

        if (info->id == 0) {
            empty_count++;
            continue;
        }

        printf("  ID=%-2u flags=0x%08x input=%-8u output=%u\n",
               info->id, info->flags, info->size_in, info->size_out);
    }

    printf("\nEmpty slots: %u\n", empty_count);

    if (empty_count > 0) {
        printf("Empty slot positions: ");
        int first = 1;

        for (uint32_t i = 0; i < result->n_commands; i++) {
            if (result->commands[i].id == 0) {
                if (!first)
                    printf(", ");

                printf("%u", i);
                first = 0;
            }
        }

        putchar('\n');
    }

    free(result);
    return 0;
}