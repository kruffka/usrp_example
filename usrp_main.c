#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "log.h"
#include "usrp_lib.h"
#include "thread_create.h"

int usrp_tx_thread;
int usrp_capture_thread;
int usrp_cap_by_timestamp;

// void readFrame(PHY_VARS_NR_UE *UE, hw_timestamp *timestamp, bool toTrash)
// {
//     void *rxp[NB_ANTENNAS_RX];

//     UE_CONFIG *ue_cfg = UE->ue_config;

//     for (int x = 0; x < 10 * ue_cfg->n_frames; x++) { // n_frames for initial sync
//         for (int slot = 0; slot < UE->frame_parms.slots_per_subframe; slot++) {
//             for (int i = 0; i < UE->frame_parms.nb_antennas_rx; i++) {
//                 if (toTrash) {
//                     rxp[i] = malloc16(2 * sizeof(int16_t) *
//                                       UE->frame_parms.get_samples_per_slot(slot, &UE->frame_parms));
//                 } else {
//                     rxp[i] =
//                         ((void *)&UE->common_vars.rxdata[i][0]) +
//                         2 * sizeof(int16_t) *
//                             ((x * UE->frame_parms.samples_per_subframe) +
//                              UE->frame_parms.get_samples_slot_timestamp(slot, &UE->frame_parms, 0));
//                 }
//             }

//             AssertFatal(UE->frame_parms.get_samples_per_slot(slot, &UE->frame_parms) ==
//                             UE->rfdevice.trx_read_func(
//                                 &UE->rfdevice,
//                                 timestamp,
//                                 rxp,
//                                 UE->frame_parms.get_samples_per_slot(slot, &UE->frame_parms),
//                                 UE->frame_parms.nb_antennas_rx),
//                         "");

//             if (toTrash) {
//                 for (int i = 0; i < UE->frame_parms.nb_antennas_rx; i++) {
//                     free(rxp[i]);
//                 }
//             }
//         }
//     }
// }


// inline static int get_readBlockSize(uint16_t slot, NR_FRAME_PARAMS *fp)
// {
//     int rem_samples = fp->get_samples_per_slot(slot, fp) - get_firstSymSamp(slot, fp);
//     int next_slot_first_symbol = 0;
//     if (slot < (fp->slots_per_frame - 1)) {
//         next_slot_first_symbol = get_firstSymSamp(slot + 1, fp);
//     }
//     return rem_samples + next_slot_first_symbol;
// }

// int ru_start(PHY_VARS_NR_UE *UE)
// {
//     UE_CONFIG *ue_cfg = UE->ue_config;
//     if (UE->rfdevice.trx_start_func(&UE->rfdevice) != 0) {
//         LOG_E(HW, "Could not start the device\n");
//         return -1;
//     }

//     // start trx write thread
//     if (usrp_tx_thread == 1) {
//         if (UE->rfdevice.trx_write_init) {
//             if (UE->rfdevice.trx_write_init(&UE->rfdevice) != 0) {
//                 LOG_E(HW, "Could not start tx write thread\n");
//                 return -1;
//             } else {
//                 LOG_I(PHY, "tx write thread ready\n");
//             }
//         }
//     }
//     return 0;
// }

// bool rx_rf(PHY_VARS_NR_UE *UE, UE_nr_rxtx_proc_t *proc)
// {
//     void *rxp[NB_ANTENNAS_RX];

//     int firstSymSamp = get_firstSymSamp(proc->nr_slot_rx, &UE->frame_parms);
//     for (int i = 0; i < UE->frame_parms.nb_antennas_rx; i++) {
//         rxp[i] =
//             (void *)&UE->common_vars
//                 .rxdata[i][firstSymSamp +
//                            UE->frame_parms
//                                .get_samples_slot_timestamp(proc->nr_slot_rx, &UE->frame_parms, 0)];
//     }

//     AssertFatal(proc->readBlockSize == UE->rfdevice.trx_read_func(&UE->rfdevice,
//                                                                   &proc->timestamp_rx,
//                                                                   rxp,
//                                                                   proc->readBlockSize,
//                                                                   UE->frame_parms.nb_antennas_rx),
//                 "Failed to read samples from device\n");

//     // Считываем из device первый символ следующего кадра для поправки timing drift
//     if (proc->nr_slot_rx == (UE->frame_parms.slots_per_frame - 1)) {
//         int first_symbols =
//             UE->frame_parms.ofdm_symbol_size + UE->frame_parms.nb_prefix_samples0; // first
//                                                                                    // symbol
//                                                                                    // of
//                                                                                    // every
//                                                                                    // frames
//         hw_timestamp ignore_timestamp;
//         AssertFatal(first_symbols == UE->rfdevice.trx_read_func(&UE->rfdevice,
//                                                                 &ignore_timestamp,
//                                                                 (void **)UE->common_vars.rxdata,
//                                                                 first_symbols,
//                                                                 UE->frame_parms.nb_antennas_rx),
//                     "Failed to read samples from device\n");
//     }

//     return true;
// }

// static bool tx_rf(PHY_VARS_NR_UE *UE, UE_nr_rxtx_proc_t *proc)
// {
//     void *txp[NB_ANTENNAS_TX];
//     for (int i = 0; i < UE->frame_parms.nb_antennas_tx; i++) {
//         txp[i] = (void *)&UE->common_vars.txdata[i][UE->frame_parms.get_samples_slot_timestamp(
//             (proc->nr_slot_tx - 1) % UE->frame_parms.slots_per_frame,
//             &UE->frame_parms,
//             0)];
//     }

//     // radio_tx_flag_t flags = Invalid;
//     int flags = MiddleOfBurst;

//     if (UE->resync == 1) {
//         flags = EndOfBurst;
//         UE->resync = 2;
//     } else if (UE->resync == 3) {
//         flags = StartOfBurst;
//         UE->resync = 0;
//     }
//     int firstSymSamp = get_firstSymSamp(proc->nr_slot_rx, &UE->frame_parms);
//     // use previous timing_advance value to compute writeTimestamp
//     if (UE->ue_config->enable_ntn) {
//         static hw_timestamp TA_adj_UE_last = 0;
//         static int was_ta_applied = 0;
//         static int ntn_dl_to_ul_slots = 0;

//         if (UE->TA_adj_UE != TA_adj_UE_last && was_ta_applied == 0) {
//             flags = StartOfBurst;
//             TA_adj_UE_last = UE->TA_adj_UE; // 122880*TA_adj_UE (мс)
//             // USRP не позволяет устанавливать timestamp для TX больше чем на 1 кадр относительно
//             // RX. Возможно не хватает длины буфера, чтобы хранить больше чем 1228800 отсчётов
//             // наперёд.
//             ntn_dl_to_ul_slots = UE->frame_parms.slots_per_frame; // 80 слотов
//             // на целевой FPGA ntn_dl_to_ul_slots = 20 мс = 20 subframe'ов = 160 слотов
//             // ntn_dl_to_ul_slots =
//             //      UE->fapi_config.extensions_config.ntn_dl_to_ul_subframes *
//             //      UE->frame_parms.slots_per_subframe;
//             was_ta_applied = 1;
//         } else if (was_ta_applied == 1) {
//             flags = EndOfBurst;
//             was_ta_applied = 2;
//         } else {
//             flags = MiddleOfBurst;
//             was_ta_applied = 0;
//         }

//         proc->timestamp_tx =
//             proc->timestamp_rx +
//             UE->frame_parms.get_samples_slot_timestamp(
//                 proc->nr_slot_rx,
//                 &UE->frame_parms,
//                 ntn_dl_to_ul_slots + dur_rx_to_tx[UE->frame_parms.numerology_index] - 1) -
//             TA_adj_UE_last - firstSymSamp - UE->hw_config[0].tx_sample_advance - UE->N_TA_offset -
//             timing_advance - UE->writetimestamp_delay; // writetimestamp_delay - to

//     } else {
//         proc->timestamp_tx = proc->timestamp_rx +
//                              UE->frame_parms.get_samples_slot_timestamp(
//                                  proc->nr_slot_rx,
//                                  &UE->frame_parms,
//                                  dur_rx_to_tx[UE->frame_parms.numerology_index] - 1) -
//                              firstSymSamp - UE->hw_config[0].tx_sample_advance - UE->N_TA_offset -
//                              timing_advance - UE->writetimestamp_delay; // writetimestamp_delay - to
//         // calibrate
//         // usrp tx
//     }

//     // but use current UE->timing_advance value to compute writeBlockSize
//     if (UE->timing_advance != timing_advance) {
//         proc->writeBlockSize -= UE->timing_advance - timing_advance;
//         timing_advance = UE->timing_advance;
//     }

//     // Отдельный поток под передачу на устройство, в этой функции лишь
//     // копируется и сигналится этот поток
//     if (usrp_tx_thread == 1) {
//         UE->rfdevice.trx_write_func(&UE->rfdevice,
//                                     proc->timestamp_tx,
//                                     txp,
//                                     proc->writeBlockSize,
//                                     UE->frame_parms.nb_antennas_tx,
//                                     flags); //, "");
//     } else {
//         // Обычный режим, когда передаем в том же потоке, что и принимаем
//         AssertFatal(
//             proc->writeBlockSize == UE->rfdevice.trx_write_func(&UE->rfdevice,
//                                                                 proc->timestamp_tx,
//                                                                 txp,
//                                                                 proc->writeBlockSize,
//                                                                 UE->frame_parms.nb_antennas_tx,
//                                                                 flags),
//             "");
//     }

// #if 0
//     FILE *file = fopen("/tmp/txdata.pcm", "a+");
//     fwrite(txp[0], proc->writeBlockSize * sizeof(int), 1, file);
//     fclose(file);
// #endif

//     return true;
// }

// bool ru_in_out_step(PHY_VARS_NR_UE *UE, UE_nr_rxtx_proc_t *proc)
// {
//     // Расчет кол-ва сэмплов для чтения/записи; Для последнего слота в кадре считываем меньше
//     // для поправки timing drift
//     if (proc->nr_slot_rx < (UE->frame_parms.slots_per_frame - 1)) {
//         proc->readBlockSize = get_readBlockSize(proc->nr_slot_rx, &UE->frame_parms);
//         proc->writeBlockSize = UE->frame_parms.get_samples_per_slot(
//             (proc->nr_slot_rx + dur_rx_to_tx[UE->frame_parms.numerology_index] - 1) %
//                 UE->frame_parms.slots_per_frame,
//             &UE->frame_parms);

//     } else {
//         UE->rx_offset_diff = computeSamplesShift(UE);
//         proc->readBlockSize =
//             get_readBlockSize(proc->nr_slot_rx, &UE->frame_parms) - UE->rx_offset_diff;
//         proc->writeBlockSize =
//             UE->frame_parms.get_samples_per_slot(
//                 (proc->nr_slot_rx + dur_rx_to_tx[UE->frame_parms.numerology_index] - 1) %
//                     UE->frame_parms.slots_per_frame,
//                 &UE->frame_parms) -
//             UE->rx_offset_diff;
//     }
//     if (!rx_rf(UE, proc)) {
//         return false;
//     }
//     if (!tx_rf(UE, proc)) {
//         return false;
//     }

//     // Отправим Slot.ind на L2/L3 для обработки текущего слота и
//     // обновлении tti на верхних уровнях
//     fapi_nr_l1_send_slot_indication(&UE->fapi.fapi_l1_endpoint, proc->frame_rx, proc->nr_slot_rx);

//     return true;
// }

double sample_rate = 10e6;
clock_source_t clock_source = internal;
clock_source_t time_source = internal;
int rx_num_channels = 1;
int tx_num_channels = 1;
uint64_t dl_carrier = 3e9;
uint64_t ul_carrier = 3e9;
double rx_gain = 30;
double tx_gain = 30;
char usrp_args[] = "addr=192.168.20.2";
int rx_channel = 0;
int tx_channel = 0;

int hw_init(hw_config_t *hw_config) {

    hw_config->sample_rate = sample_rate;
    hw_config->clock_source = clock_source;
    hw_config->time_source = time_source;
    hw_config->rx_num_channels = rx_num_channels;
    hw_config->tx_num_channels = tx_num_channels;

    LOG_D("Configuring sample_rate %f, tx/rx num_channels %d/%d\n",
        hw_config->sample_rate,
        hw_config->tx_num_channels,
        hw_config->rx_num_channels);

    for (int i = 0; i < 4; i++) {
        if (i < hw_config->rx_num_channels) {
            hw_config->rx_freq[i] = dl_carrier;
        } else {
            hw_config->rx_freq[i] = 0.0;
        }
    
        if (i < hw_config->tx_num_channels) {
            hw_config->tx_freq[i] = ul_carrier;
        } else {
            hw_config->tx_freq[i] = 0.0;
        }

        hw_config->rx_gain[i] = rx_gain;
        hw_config->tx_gain[i] = tx_gain;
    }
    // Канал в UHD для USRP N300 задаётся в формате "A:0" или "A:1"
    // Номер Rx канала 0 или 1 преобразуется в строку "A:0" или "A:1"
    char rx_channel_str[3];
    snprintf(rx_channel_str, sizeof(rx_channel_str), "%d", rx_channel);
    char rx_subdev_str[5] = "A:";
    strcat(rx_subdev_str, rx_channel_str);
    hw_config->rx_subdev = malloc(strlen(rx_subdev_str) + 1);
    strcpy(hw_config->rx_subdev, rx_subdev_str);
    // Номер Tx канала 0 или 1 преобразуется в строку "A:0" или "A:1"
    char tx_channel_str[3];
    snprintf(tx_channel_str, sizeof(tx_channel_str), "%d", tx_channel);
    char tx_subdev_str[5] = "A:";
    strcat(tx_subdev_str, tx_channel_str);
    hw_config->tx_subdev = malloc(strlen(tx_subdev_str) + 1);
    strcpy(hw_config->tx_subdev, tx_subdev_str);
    hw_config->sdr_addrs = (char *)&(usrp_args);

    return 0;
}

int main(void) {

    hw_device rfdevice = {0};
    hw_config_t hw_config = {0};

    set_latency_target();
    if (getuid() == 0) {
        set_priority(PRIORITY_RT_MAX);
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
            fprintf(stderr, "mlockall: %s\n", strerror(errno));
        }
    }

    if (log_init("/tmp/test", MAX_LOG_LEVEL) < 0) {
        fprintf(stderr, "error init logs\n");
        return -1;
    }

    LOG_I("Version: %s\n", GIT_VERSION);

    if (hw_init(&hw_config) < 0) {
        LOG_E("HW init error\n");
        return -1;
    }

    LOG_I("Init device\n");
    if (device_init(&rfdevice, &hw_config)) {
        LOG_E("Could not init the usrp device\n");
        return -1;
    }

    // if (ru_start(UE) == 0) {
    //     LOG_I(HW, "RU started\n");
    // } else {
    //     LOG_E(HW, "Failed to start RU\n");
    //     ue_exit = 1;
    // }

    // UE->rfdevice.trx_stop_func(&UE->rfdevice);


    log_deinit();
    
    printf("bye\n");
}