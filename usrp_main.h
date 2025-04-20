#ifndef _USRP_MAIN_H_
#define _USRP_MAIN_H_

#include "log.h"
#include "usrp_lib.h"
#include "thread_create.h"

#define CAPABILITY_SLOT_RX_TO_TX (4)

typedef struct trx_proc {
    
    int rx_pos;
    int tx_pos;

    uint32_t **rxdata;
    uint32_t **txdata;

    hw_timestamp timestamp_rx;
    hw_timestamp timestamp_tx;

#define NOF_BLOCKS_IN_BUF (20)
    int readBlockSize;
    int writeBlockSize;

    int nb_antennas_rx;
    int nb_antennas_tx;

} trx_proc_t;

#endif // _USRP_MAIN_H_