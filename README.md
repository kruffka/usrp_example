## Open source

В основе usrp_lib.cc и некоторых вспомогательных функций взят код из (https://gitlab.eurecom.fr/oai/openairinterface5g)  

## Зависимости    

Первый делом для работы с USRP понадобится UHD библиотека и все ее зависимости, поэтому если ее еще не установили, то идем сюда и собираем из исходников: [Инструкция по установке UHD](https://github.com/kruffka/usrp_example/blob/master/docs/UHD_install.md)         

## Сборка

Сборка примеров с USRP:   
```bash
mkdir build && cd build
cmake ..
make
```

После сборки в каталоге build/examples появятся несколько исполняемых файлов:
- usrp_example - Основной пример, работающий с TX и RX одновременно
- usrp_rx_to_file - Пример для записи в файл данных с RX USRP
- usrp_tx_from_file - Пример для отправки данных с файла на TX USRP

## Запуск

Можно запускать без sudo, но можно потерять в производительности для больших sample rate и получить много Overflow (O) или UnderFlow (U) от USRP         
```bash
sudo ./examples/usrp_example --llevel=4
```

```bash
sudo ./examples/usrp_rx_to_file --file_rx=/tmp/rxdata0.pcm
```

```bash
sudo ./examples/usrp_tx_from_file --file_rx=/tmp/txdata0.pcm
```

Каждый из исполняемых файлов имеет конфигурацию, все поля конфигурации можно вывести на экран так:      
```bash
sudo ./examples/usrp_example -h
```


## Описание кода

Как работает код, что он может и что в нем не очень [здесь](https://github.com/kruffka/usrp_example/blob/master/docs/README.md)
