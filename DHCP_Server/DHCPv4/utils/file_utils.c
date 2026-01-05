#include <stdlib.h>
#include <string.h>
#include "../include/file_utils.h"

int advance_to_next_closed_brace(FILE *fp)
{
    if (!fp)
        return -1;

    int ch;
    int brace_count = 0;
    
    while ((ch = fgetc(fp)) != EOF)
    {
        if (ch == '{')
        {
            brace_count++;
        }
        else if (ch == '}')
        {
            if (brace_count == 0)
            {
                return 0; // Found closing brace without matching opening brace
            }
            brace_count--;
        }
    }

    return -1; // Reached EOF without finding unmatched closing brace
}