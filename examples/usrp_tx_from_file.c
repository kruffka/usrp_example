#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "usrp_main.h"
#include "common/config.h"

int usrp_tx_thread;
int usrp_capture_thread;
int usrp_cap_by_timestamp;


static int ru_start(hw_device *rfdevice)
{
    if (rfdevice->trx_start_func(rfdevice) != 0) {
        LOG_E("Could not start the device\n");
        return -1;
    }

    // start trx write thread
    if (usrp_tx_thread == 1) {
        if (rfdevice->trx_write_init) {
            if (rfdevice->trx_write_init(rfdevice) != 0) {
                LOG_E("Could not start tx write thread\n");
                return -1;
            } else {
                LOG_I("tx write thread ready\n");
            }
        }
    }
    return 0;
}

static bool tx_rf(hw_device *rfdevice, trx_proc_t *proc)
{
    void *txp[proc->nb_antennas_tx];
    for (int i = 0; i < proc->nb_antennas_tx; i++) {
        txp[i] = (void *)&proc->txdata[i][proc->tx_pos * proc->writeBlockSize];
    }

    // другие флаги необходимо использовать лишь тогда, когда нужно выкл/вкл tx
    int flags = MiddleOfBurst;

    proc->timestamp_tx = proc->timestamp_rx - rfdevice->hw_config->tx_sample_advance + CAPABILITY_SLOT_RX_TO_TX * proc->readBlockSize; 

    // Отдельный поток под передачу на устройство, в этой функции лишь
    // копируется и сигналится этот поток
    if (usrp_tx_thread == 1) {
        rfdevice->trx_write_func(rfdevice, proc->timestamp_tx, txp, proc->writeBlockSize, proc->nb_antennas_tx, flags);
    } else {
        // Обычный режим, когда передаем в том же потоке, что и принимаем
        if (proc->writeBlockSize != rfdevice->trx_write_func(rfdevice, proc->timestamp_tx, txp, proc->writeBlockSize, proc->nb_antennas_tx, flags)) {
            LOG_W("write block size != sent block size\n");
            return false;
        }
    }

    LOG_D("USRP tx: timestamp rx %ld, tx %ld, diff %ld, size %d, pos %d/%d\n", proc->timestamp_rx, proc->timestamp_tx, proc->timestamp_tx - proc->timestamp_rx, proc->writeBlockSize, proc->tx_pos, NOF_BLOCKS_IN_BUF);

    proc->tx_pos = (proc->tx_pos + 1) % NOF_BLOCKS_IN_BUF;

    return true;
}

int main(int argc, char *argv[]) {

    hw_device rfdevice = {0};

    // Для скорости - работает лишь под sudo
    if (getuid() == 0) {
        set_latency_target();
        set_priority(PRIORITY_RT_MAX);
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
            fprintf(stderr, "mlockall: %s\n", strerror(errno));
        }
    }

    // Мини парсер полезных опций
    config_t cfg = {};
    parse_config(argc, argv, &cfg);
    printf("  File tX: %s\n", cfg.file_tx);
    FILE *file = fopen(cfg.file_tx, "rb");
    if (file == NULL) {
        printf("File path is NULL: use --file_tx <path> option\n");
        goto exit;
    }

    // Инициализация логов
    if (log_init(cfg.lpath, cfg.llevel) < 0) {
        fprintf(stderr, "error init logs\n");
        goto exit;
    }

    LOG_I("Version: %s\n", GIT_VERSION);

    hw_config_t hw_config = {0};
    trx_proc_t proc = {0};

    // Заполнение hw_config и proc из cfg
    if (hw_config_init(&hw_config, &proc, &cfg) < 0) {
        LOG_E("HW init error\n");
        goto exit;
    }

    LOG_I("Init device\n");
    // Инициализация rfdevice с помощью hw_config'а
    if (device_init(&rfdevice, &hw_config)) {
        LOG_E("Could not init the usrp device\n");
        goto exit;
    }

    // Запуск HW
    if (ru_start(&rfdevice) == 0) {
        LOG_I("RU started\n");
    } else {
        LOG_E("Failed to start RU\n");
        goto exit;
    }



    while (!feof(file)) {
        // и подготовка TX: где i - номер антенны, tx_pos - текущий номер слота, writeBlockSize - размер слота
        // &proc->txdata[i][proc->tx_pos * proc->writeBlockSize]

        int read = fread(&proc.txdata[0][proc.tx_pos * proc.writeBlockSize], proc.writeBlockSize, sizeof(int), file);
        if (read != proc.writeBlockSize) {
            break;
        }

        tx_rf(&rfdevice, &proc);

    }
    fclose(file);

    // Остановка HW
    rfdevice.trx_stop_func(&rfdevice);

exit:
    log_deinit();
    // hw_deinit();
    
    printf("bye\n");
}