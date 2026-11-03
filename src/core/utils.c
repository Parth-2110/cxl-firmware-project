#include "cxl_fw.h"

int parse_uint(const char *text, unsigned int *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == '\0' ||
        text[0] == '-')
        return -1;

    errno = 0;
    parsed = strtoul(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        parsed == 0 || parsed > UINT32_MAX)
        return -1;

    *value = (unsigned int)parsed;
    return 0;
}