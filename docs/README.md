## Опции

Для всех примеров используются следующии опции:      
![image](https://github.com/user-attachments/assets/b9c5a2b3-4612-4ec9-bb99-a5c759132271)       
Увидеть опции можно с помощью флага **-h**:     
```bash
./examples/usrp_example -h
```

Только для **usrp_rx_to_file** обязательная опция **--file_rx**, пример:
```bash
sudo ./examples/usrp_rx_to_file --file_rx=/tmp/rxdata0.pcm
```
где после **file_rx** задается путь до файла, в который будем записывать данные с USRP           

Только для **usrp_tx_from_file** обязательная опция **--file_tx**, пример:     
```bash
sudo ./examples/usrp_tx_from_file --file_rx=/tmp/txdata0.pcm
```
где после **file_tx** идет путь до файла, с которого будем читать данные       

Для остальных примеров эти опции не используются      

**При записи для больших sample rate код не будет успевать, т.к. запись происходит в том же потоке, что и чтение с USRP. Тоже самое касается и tx примера**    

## Код
Разберем example/usrp_example.c

### Инициализация

- **parse_config()** - Парсит входные аргументы программы и применяет их. В случае отсутствия аргументов применяет значения по умолчанию, которые можно увидеть запустив программу с ключом -h
- **log_init()** - Инициализация логов. Реализация логов здесь простая и само логирование сделано в том же потоке, что их и вызывает, таким образом при достаточно частом выводе логов на экран поток может замедляться
- **hw_config_init()** - Заполняются структуры **hw_config_t hw_config** и **trx_proc_t proc**. hw_config необходим для инициализации структуры hw_device rfdevice для работы с USRP. Структура proc нужна для передачи/приема и обработки данных с USRP

### Функции с USRP

- **device_init()** - Инициализирует rfdevice структуру параметрами из hw_device, а также готовим USRP к работе. После инициализации USRP будет залочена за нами
- **ru_start()** - Запуск USRP
- **trx_stop_func()** - Остановка USRP
- **trx_read_func()** - Прием данных из USRP, блокирующая ф-ия, т.к. будем ждать пока заданное кол-во данных нами не прилетит из воздуха через антенну
- **trx_write_func()** - Передача данных на USRP, блокирующая если usrp_tx_thread = 0 и не блокирующая если usrp_tx_thread = 1. **Очищает буфер после отправки**

## RX и TX
Для работы с RX и TX применим кольцевой буфер. Рассмотрим буфер rxdata (txdata такой же):       
uint32_t **rxdata - наш кольцевой буфер для работы с RX USRP. 
- rxdata - указатель на RX буфер во временной области
- rxdata[0] - первая антенна
- rxdata[0][0] - первая антенна и первый uint32_t сэмпл, где первые 16 бит будут RE, вторые IM


Для работы с такими TRX буферами используется структура trx_proc_t proc, в которой хранится размер блока для чтения/записи, а также текущаяя позиция в кольцевом буфере.
```c
int rx_pos;
int tx_pos;

int readBlockSize;
int writeBlockSize;
```
Размер кольцевого буфера захардкожен в define:
```c
#define NOF_BLOCKS_IN_BUF (20)
```

tx_pos как и rx_pos меняется на единицу после каждой записи/чтения и принимает значения в диапазоне [0, NOF_BLOCKS_IN_BUF].

```c
// Обработка RX: где i - номер антенны, rx_pos - текущий номер слота, readBlockSize - размер слота
&proc->rxdata[i][proc->rx_pos * proc->readBlockSize]

// и подготовка TX: где i - номер антенны, tx_pos - текущий номер слота, writeBlockSize - размер слота
&proc->txdata[i][proc->tx_pos * proc->writeBlockSize]
```

Таким образом мы имеем кольцевой буфер, в который по кругу читаются/записываются данные с USRP и затем мы их можем обработать либо скопировать куда-то еще и обработать паралеллельно позже


### rx_rf()

rxp хранит указатель на текущую позицию в кольцевом буфере. Функцией trx_read_func мы эти данные записываем по адресу rxp. В proc->timestamp_rx мы получаем текущий timestamp RX от USRP. proc->readBlockSize - размер нашего блока на чтение
```c
    void *rxp[proc->nb_antennas_rx];

    for (int i = 0; i < proc->nb_antennas_rx; i++) {
        rxp[i] = (void *)&proc->rxdata[i][proc->rx_pos * proc->readBlockSize];
    }
    int readSize = rfdevice->trx_read_func(rfdevice,
                                            &proc->timestamp_rx,
                                            rxp,
                                            proc->readBlockSize,
                                            proc->nb_antennas_rx);
```

USRP после запуска постоянно читает данные с воздуха и кладет к себе в буфер, мы же этой функцией trx_read_func копируем эти данные из буфера к себе. Если мы будем слишком медленно читать, то буфер на USRP переполнится и мы потеряем данные, **в таком случае readSize не будет = proc->readBlockSize**
Если мы читаем слишком быстро, то мы заблокируемся на ожидании данных с USRP.

### tx_rf()

Здесь есть пару отличий от RX:
- timestamp мы задаем самостоятельно и отдаем его на USRP
- timestamp мы должны формировать наперед, т.к. если наш timestamp_tx будет меньше текущего timestamp_rx, то мы будто пытаемся отправить данные в прошлое
- Отправку можно делать в отдельном потоке, т.к. если попытаться постоянно отправлять в далекое будущее, то мы заблокируемся, т.к. на USRP также есть буфер на отправку и он не бесконечный

Поэтому обычно при расчете timestamp_tx берут за основу временную метку от RX - timestamp_rx    
```c
 void *txp[proc->nb_antennas_tx];
    for (int i = 0; i < proc->nb_antennas_tx; i++) {
        txp[i] = (void *)&proc->txdata[i][proc->tx_pos * proc->writeBlockSize];
    }
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

```
После отправки память очистится в функции trx_write_func

