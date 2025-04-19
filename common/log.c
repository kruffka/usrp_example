#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "log.h"

#define YELLOW "\e[0;33m"
#define RED "\e[0;31m"
#define GREEN "\e[0;32m"
#define BLUE "\e[0;34m"
#define RESET "\e[0m"

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
int my_log(log_type_e type, char *type_s, char *str) {

    if (type >= MAX_LOG_LEVEL) {
        return -1;
    }

    if (type > log_g.level) {
        return 0;
    }

    if (!log_g.is_running) {
        return -1;
    }

    time_t mytime = time(NULL);
    char * time_str = ctime(&mytime);
    time_str[strlen(time_str)-1] = '\0';

    char *color_array[MAX_LOG_LEVEL] = {GREEN, YELLOW, RED, BLUE};
    char output[1024];
    snprintf(output, sizeof(output), "[%s][%s][%d] %s%s"RESET, time_str, type_s, log_g.counter, color_array[type], str);
    printf("%s", output);
    if (log_g.file) {
        fprintf(log_g.file, "%s", output);
    }
    log_g.counter++;

    return 0;  

}
