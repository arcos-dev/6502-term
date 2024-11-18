// logging.c
#include "logging.h"

/* Log a debug message to a file */
void log_debug(const char *format, ...)
{
    FILE *log_file = fopen("emulator.log", "a");

    if (!log_file)
        return;

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fclose(log_file);
}
