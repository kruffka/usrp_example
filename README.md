## Open source

В основе usrp_lib.cc и некоторых вспомогательных функций взят код из (https://gitlab.eurecom.fr/oai/openairinterface5g)  

## Зависимости    

Первый делом для работы с USRP понадобится UHD библиотека и все ее зависимости, поэтому если ее еще не установили, то идем сюда и собираем из исходников: [Инструкция по установке UHD](https://github.com/kruffka/usrp_example/blob/master/docs/UHD_install.md)         

## Опционально
Для больших sample rate, софт для работы с usrp может не успевать и будут возникать OOO или UUU (Overflow, Underrun и прочие). Чтобы сократить (или вовсе их убрать) следует выполнить следующие шаги:
- Выставить одинаковое MTU на USRP и на хосте, например на 9000
- Поставить RT ядро (см. в гугле), при вводе uname -a, должно выводить -rt в версии ядра
- Выставить максимальную частоту в CPU для всех ядер **(в текущих примерах это делается если запустить под sudo)**
- Governor
```bash
  echo performance | sudo tee /sys/devices/system/cpu/cpufreq/policy*/scaling_governor
  cpufreq-info
```
- Включить изоляцию ядер для нашего софта в GRUB (Например: GRUB_CMDLINE_LINUX_DEFAULT="isolcpus=16-31 intel_pstate=disable processor.max_cstate=1 intel_idle.max_cstate=0 idle=poll iommu=pt intel_iommu=on hugepages=2048 nohz_full=16-31 rcu_nocbs=16-31")

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
