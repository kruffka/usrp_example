#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "config.h"

#ifndef malloc16
    #ifdef __AVX2__
        #define malloc16(x) memalign(32, x)
    #else
        #define malloc16(x) memalign(16, x)
    #endif
#endif

inline static void *malloc16_clear(size_t size)
{
#ifdef __AVX2__
    void *ptr = memalign(32, size);
#else
    void *ptr = memalign(16, size);
#endif
    memset(ptr, 0, size);
    return ptr;
}

extern int usrp_tx_thread;

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -r, --sample-rate <Hz>        Sample rate (default: 30.72e6)\n");
    printf("  -c, --clock-source <src>      Clock source [internal|external|gpsdo] (default: internal)\n");
    printf("  -t, --time-source <src>       Time source [internal|external|gpsdo] (default: internal)\n");
    printf("  -R, --rx-channels <num>       Number of RX channels (default: 1)\n");
    printf("  -T, --tx-channels <num>       Number of TX channels (default: 1)\n");
    printf("  -d, --dl-carrier <Hz>         Downlink carrier frequency (default: 3e9)\n");
    printf("  -u, --ul-carrier <Hz>         Uplink carrier frequency (default: 3e9)\n");
    printf("  -g, --rx-gain <dB>            RX gain (default: 30)\n");
    printf("  -G, --tx-gain <dB>            TX gain (default: 30)\n");
    printf("  -a, --usrp-args <args>        USRP device arguments (default: \"addr=192.168.20.2\")\n");
    printf("  -x, --rx-channel <num>        RX channel number (default: 0)\n");
    printf("  -y, --tx-channel <num>        TX channel number (default: 0)\n");
    printf("  -U, --usrp_tx_thread <num>    Use thread for TX USRP (use sudo for this) [0 - no, 1 - yes] (default: 0)\n");
    printf("  -L, --llevel <num>            Log level [0 - ERR, 1 - WARN, 2 - INFO, 3 - DBG] (default: INFO)\n");
    printf("  -l, --lpath <path>            Log path (default: NULL)\n");
    printf("  -h, --help                    Show this help message\n");
}

// Long options
static struct option long_options[] = {
    {"sample-rate", required_argument, 0, 'r'},
    {"clock-source", required_argument, 0, 'c'},
    {"time-source", required_argument, 0, 't'},
    {"rx-channels", required_argument, 0, 'R'},
    {"tx-channels", required_argument, 0, 'T'},
    {"dl-carrier", required_argument, 0, 'd'},
    {"ul-carrier", required_argument, 0, 'u'},
    {"rx-gain", required_argument, 0, 'g'},
    {"tx-gain", required_argument, 0, 'G'},
    {"usrp-args", required_argument, 0, 'a'},
    {"rx-channel", required_argument, 0, 'x'},
    {"tx-channel", required_argument, 0, 'y'},
    {"usrp_tx_thread", required_argument, 0, 'U'},
    {"llevel", required_argument, 0, 'L'},
    {"lpath", required_argument, 0, 'l'},
    {"file_rx", required_argument, 0, 'F'},
    {"file_tx", required_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

clock_source_t parse_clock_source(const char* src) {
    if (strcmp(src, "internal") == 0) return internal;
    if (strcmp(src, "external") == 0) return external;
    if (strcmp(src, "gpsdo") == 0) return gpsdo;
    return internal;
}

// Конфиг без проверки на некорректные опции, оставим это на usrp
int parse_config(int argc, char *argv[], config_t *config) {

    // значения по умолчанию
    config->sample_rate = 30.72e6;
    config->clock_source = internal;
    config->time_source = internal;
    config->rx_num_channels = 1;
    config->tx_num_channels = 1;
    config->dl_carrier = 3e9;
    config->ul_carrier = 3e9;
    config->rx_gain = 30;
    config->tx_gain = 30;
    strncpy(config->usrp_args, "addr=192.168.20.2", sizeof(config->usrp_args) - 1);
    config->usrp_args[sizeof(config->usrp_args) - 1] = '\0';
    config->rx_channel = 0;
    config->tx_channel = 0;
    config->usrp_tx_thread = 0;
    config->llevel = INFO;
    config->lpath = NULL;
    config->file_tx = NULL;
    config->file_rx = NULL;

    int opt;
    while ((opt = getopt_long(argc, argv, "r:c:t:R:T:d:u:g:G:a:x:y:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                config->sample_rate = atof(optarg);
                break;
            case 'c':
                config->clock_source = parse_clock_source(optarg);
                break;
            case 't':
                config->time_source = parse_clock_source(optarg);
                break;
            case 'R':
                config->rx_num_channels = atoi(optarg);
                break;
            case 'T':
                config->tx_num_channels = atoi(optarg);
                break;
            case 'd':
                config->dl_carrier = strtoull(optarg, NULL, 10);
                break;
            case 'u':
                config->ul_carrier = strtoull(optarg, NULL, 10);
                break;
            case 'g':
                config->rx_gain = atof(optarg);
                break;
            case 'G':
                config->tx_gain = atof(optarg);
                break;
            case 'a':
                strncpy(config->usrp_args, optarg, sizeof(config->usrp_args) - 1);
                config->usrp_args[sizeof(config->usrp_args) - 1] = '\0';
                break;
            case 'x':
                config->rx_channel = atoi(optarg);
                break;
            case 'y':
                config->tx_channel = atoi(optarg);
                break;
            case 'U':
                config->usrp_tx_thread = atoi(optarg);
                break;
            case 'l':
                config->lpath = optarg;
                break;
            case 'L':
                config->llevel = atoi(optarg);
                break;
            case 'F':
                config->file_rx = optarg;
                break;
            case 'f':
                config->file_tx = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case '?':
                // Unknown option or missing argument
                print_usage(argv[0]);
                exit(0);
        }
    }

    // Print all parameters for verification
    printf("Configuration:\n");
    printf("  Sample rate: %.2f Hz\n", config->sample_rate);
    printf("  Clock source: %d\n", config->clock_source);
    printf("  Time source: %d\n", config->time_source);
    printf("  RX channels: %d\n", config->rx_num_channels);
    printf("  TX channels: %d\n", config->tx_num_channels);
    printf("  DL carrier: %llu Hz\n", (unsigned long long)config->dl_carrier);
    printf("  UL carrier: %llu Hz\n", (unsigned long long)config->ul_carrier);
    printf("  RX gain: %.1f dB\n", config->rx_gain);
    printf("  TX gain: %.1f dB\n", config->tx_gain);
    printf("  USRP args: %s\n", config->usrp_args);
    printf("  RX channel: %d\n", config->rx_channel);
    printf("  TX channel: %d\n", config->tx_channel);
    printf("  USRP TX thread: %d\n", config->usrp_tx_thread);
    printf("  Log level: %d\n", config->llevel);
    printf("  Log file: %s\n", config->lpath);

    return 0;
}


int hw_config_init(hw_config_t *hw_config, trx_proc_t *proc, config_t *cfg) {

    usrp_tx_thread = cfg->usrp_tx_thread;

    hw_config->sample_rate = cfg->sample_rate;
    hw_config->clock_source = cfg->clock_source;
    hw_config->time_source = cfg->time_source;
    hw_config->rx_num_channels = cfg->rx_num_channels;
    hw_config->tx_num_channels = cfg->tx_num_channels;

    LOG_I("Configuring sample_rate %f, tx/rx num_channels %d/%d\n",
        hw_config->sample_rate,
        hw_config->tx_num_channels,
        hw_config->rx_num_channels);

    for (int i = 0; i < 4; i++) {
        if (i < hw_config->rx_num_channels) {
            hw_config->rx_freq[i] = cfg->dl_carrier;
        } else {
            hw_config->rx_freq[i] = 0.0;
        }
    
        if (i < hw_config->tx_num_channels) {
            hw_config->tx_freq[i] = cfg->ul_carrier;
        } else {
            hw_config->tx_freq[i] = 0.0;
        }

        hw_config->rx_gain[i] = cfg->rx_gain;
        hw_config->tx_gain[i] = cfg->tx_gain;
    }
    // Канал в UHD для USRP N300 задаётся в формате "A:0" или "A:1"
    // Номер Rx канала 0 или 1 преобразуется в строку "A:0" или "A:1"
    char rx_channel_str[3];
    snprintf(rx_channel_str, sizeof(rx_channel_str), "%d", cfg->rx_channel);
    char rx_subdev_str[5] = "A:";
    strcat(rx_subdev_str, rx_channel_str);
    hw_config->rx_subdev = malloc(strlen(rx_subdev_str) + 1);
    strcpy(hw_config->rx_subdev, rx_subdev_str);
    // Номер Tx канала 0 или 1 преобразуется в строку "A:0" или "A:1"
    char tx_channel_str[3];
    snprintf(tx_channel_str, sizeof(tx_channel_str), "%d", cfg->tx_channel);
    char tx_subdev_str[5] = "A:";
    strcat(tx_subdev_str, tx_channel_str);
    hw_config->tx_subdev = malloc(strlen(tx_subdev_str) + 1);
    strcpy(hw_config->tx_subdev, tx_subdev_str);
    hw_config->sdr_addrs = (char *)&(cfg->usrp_args);


    proc->readBlockSize = cfg->sample_rate / 1000.0;
    proc->writeBlockSize = proc->readBlockSize;
    proc->nb_antennas_rx = cfg->rx_num_channels;
    proc->nb_antennas_tx = cfg->tx_num_channels;

    proc->rx_pos = proc->tx_pos = 0;
    proc->timestamp_rx = proc->timestamp_tx = 0;

    proc->rxdata = (uint32_t **)malloc16(proc->nb_antennas_rx * sizeof(uint32_t *));
    proc->txdata = (uint32_t **)malloc16(proc->nb_antennas_tx * sizeof(uint32_t *));
    
    if ((proc->rxdata == NULL) || (proc->txdata == NULL)) {
        LOG_E("Out of memory: rxdata %p txdata %p\n", proc->rxdata, proc->txdata);
        return -1;
    }

    LOG_I("Init rxdata buf with size of %d, block size %d, nb ant %d\n", proc->readBlockSize * NOF_BLOCKS_IN_BUF, proc->readBlockSize, proc->nb_antennas_rx);
    for (int i = 0; i < proc->nb_antennas_rx; i++) {
        proc->rxdata[i] = (uint32_t *)malloc16_clear(proc->readBlockSize * NOF_BLOCKS_IN_BUF * sizeof(int32_t));
        if (proc->rxdata[i] == NULL) {
            LOG_E("Out of memory: rxdata %p ant %d\n", proc->rxdata, i);
            return -1;
        }
        memset(proc->rxdata[i], 0, proc->readBlockSize * NOF_BLOCKS_IN_BUF * sizeof(int32_t));
    }

    LOG_I("Init txdata buf with size of %d, block size %d, nb ant %d\n", proc->writeBlockSize * NOF_BLOCKS_IN_BUF, proc->writeBlockSize, proc->nb_antennas_tx);
    for (int i = 0; i < proc->nb_antennas_rx; i++) {
        proc->txdata[i] = (uint32_t *)malloc16_clear(proc->writeBlockSize * NOF_BLOCKS_IN_BUF * sizeof(int32_t));
        if (proc->txdata[i] == NULL) {
            LOG_E("Out of memory: txdata %p ant %d\n", proc->txdata, i);
            return -1;
        }
        memset(proc->txdata[i], 0, proc->writeBlockSize * NOF_BLOCKS_IN_BUF * sizeof(int32_t));
    }

    return 0;
}
