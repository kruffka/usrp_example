#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include "log.h"

static char *color_array[MAX_LOG_LEVEL] = {RED, YELLOW, RESET, BLUE};

typedef struct log_s {
    FILE *file;
    bool is_running;
    unsigned int counter;
    log_type_e level;
} log_t;

static log_t log_g;

int log_init(char *filepath, log_type_e log_level) {

    if (log_g.is_running) {
        return -1;
    }
    if (filepath) {
        log_g.file = fopen(filepath, "w");
        if (log_g.file == NULL) {
            return -1;
        }
    }
    log_g.counter = 0;
    log_g.is_running = true;
    log_g.level = log_level;

    return 0;
}

void log_deinit(void) {
    if (log_g.file) fclose(log_g.file);
    log_g.is_running = false;
}  

// Моя простая функция для логирования в текущем потоке
int my_log(const log_type_e type, const char *type_s, const char *fmt, ...) {

    if (type >= MAX_LOG_LEVEL) {
        return -1;
    }

    if (type > log_g.level) {
        return 0;
    }

    if (!log_g.is_running) {
        return -1;
    }

    char tmp[512];
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, arg_ptr);
    va_end(arg_ptr);

    time_t mytime = time(NULL);
    char * time_str = ctime(&mytime);
    time_str[strlen(time_str)-1] = '\0';

    char output[1024];
    snprintf(output, sizeof(output), "[%s][%s][%d] %s%s"RESET, time_str, type_s, log_g.counter, color_array[type], tmp);
    fputs(output, stdout);
    if (log_g.file) {
        fputs(output, log_g.file);
    }
    log_g.counter++;

    return 0;  

}