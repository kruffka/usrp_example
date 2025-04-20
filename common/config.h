#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "usrp_main.h"
#include "usrp/usrp_lib.h"


typedef struct config {
    double sample_rate;
    clock_source_t clock_source;
    clock_source_t time_source;
    int rx_num_channels;
    int tx_num_channels;
    uint64_t dl_carrier;
    uint64_t ul_carrier;
    double rx_gain;
    double tx_gain;
    char usrp_args[1024];
    int rx_channel;
    int tx_channel;
    int usrp_tx_thread;

    // debug
    log_type_e llevel;
    char *lpath;
    
} config_t;

int parse_config(int argc, char *argv[], config_t *config);

int hw_config_init(hw_config_t *hw_config, trx_proc_t *proc, config_t *cfg);

#endif // _CONFIG_H_
