#ifndef _LOG_H_
#define _LOG_H_

#define YELLOW "\e[0;33m"
#define RED "\e[0;31m"
#define GREEN "\e[0;32m"
#define BLUE "\e[0;34m"
#define RESET "\e[0m"

#define _PRINT_LOG(type, fmt, ...)                              \
    do {                                                        \
        my_log(type, #type, fmt, ##__VA_ARGS__);                  \
    } while(0)              

#define LOG_I(str, ...) _PRINT_LOG(INFO, str, ##__VA_ARGS__)  
#define LOG_D(str, ...) _PRINT_LOG(DEBUG, str, ##__VA_ARGS__)  
#define LOG_E(str, ...) _PRINT_LOG(ERROR, str, ##__VA_ARGS__)  
#define LOG_W(str, ...) _PRINT_LOG(WARNING, str, ##__VA_ARGS__)  

typedef enum log_color {
    ERROR = 0,  // красный
    WARNING,    // жёлтый
    INFO,       // без цвета
    DEBUG,      // синий
    MAX_LOG_LEVEL
} log_type_e;

// Моя простая функция для логирования в текущем потоке
int my_log(log_type_e type, char *type_s, char *fmt, ...);

int log_init(char *filepath, log_type_e log_level);

void log_deinit(void);

#endif // _LOG_H_