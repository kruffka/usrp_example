#ifndef _LOG_H_
#define _LOG_H_

// #define MY_LOG2(type, str) my_log(type, #type, str)

#define LOG_I(str) my_log(INFO, "INFO", str)
#define LOG_D(str) my_log(DEBUG, "DEBUG", str)
#define LOG_E(str) my_log(ERROR, "ERROR", str)

typedef enum log_color {
    INFO = 0, // зеленый
    WARNING, // жёлтый
    ERROR, // красный
    DEBUG, // синий
    MAX_LOG_LEVEL
} log_type_e;

// Моя простая функция для логирования в текущем потоке
int my_log(log_type_e type, char *type_s, char *str) ;

int log_init(char *filepath, log_type_e log_level);

void log_deinit(void);

#endif // _LOG_H_