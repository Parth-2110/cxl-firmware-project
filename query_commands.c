// Retrieves and displays CXL firmware slot information, activation capabilities, and firmware revisions.

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/cxl_mem.h>

int main(void)
{
    int fd;
    struct cxl_mem_query_commands count_query = {0};
    struct cxl_mem_query_commands *query;
    size_t query_size;
    unsigned int count;

    fd = open("/dev/cxl/mem0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/cxl/mem0");
        return EXIT_FAILURE;
    }

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, &count_query) < 0) {
        perror("CXL_MEM_QUERY_COMMANDS (count)");
        close(fd);
        return EXIT_FAILURE;
    }

    count = count_query.n_commands;
    printf("Number of supported commands: %u\n", count);

    if (count == 0) {
        printf("No supported commands were reported.\n");
        close(fd);
        return EXIT_SUCCESS;
    }

    query_size = sizeof(*query) +
                 count * sizeof(struct cxl_command_info);

    query = calloc(1, query_size);
    if (query == NULL) {
        perror("calloc");
        close(fd);
        return EXIT_FAILURE;
    }

    query->n_commands = count;

    if (ioctl(fd, CXL_MEM_QUERY_COMMANDS, query) < 0) {
        perror("CXL_MEM_QUERY_COMMANDS (list)");
        free(query);
        close(fd);
        return EXIT_FAILURE;
    }

    printf("\nSupported CXL commands:\n");
    printf("%-8s %-12s %-15s %-15s\n",
           "ID", "Flags", "Input size", "Output size");

   printf("\n%-6s %-24s %-10s %-12s %-12s\n",
       "ID", "Command", "Enabled", "Input size", "Output size");

for (unsigned int i = 0; i < query->n_commands; i++) {
    struct cxl_command_info *cmd = &query->commands[i];

    if (cmd->id == 0)
        continue;

    printf("%-6u %-24s %-10s %-12u %-12u\n",
           cmd->id,
           cmd->id == 4 ? "GET_FW_INFO" :
           cmd->id == 1 ? "IDENTIFY" :
           cmd->id == 2 ? "RAW" : "Other",
           (cmd->flags & 0x1) ? "Yes" : "No",
           cmd->size_in,
           cmd->size_out);
}

    free(query);
    close(fd);
    return EXIT_SUCCESS;
}
