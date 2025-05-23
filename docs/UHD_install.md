

## Установка UHD на хост
Ставим пакеты, необходимые для сборки libuhd:

```bash
sudo apt-get install autoconf automake build-essential ccache cmake cpufrequtils doxygen ethtool g++ git inetutils-tools libboost-all-dev libncurses5 libncurses5-dev libusb-1.0-0 libusb-1.0-0-dev libusb-dev python3-dev python3-mako python3-numpy python3-requests python3-scipy python3-setuptools python3-ruamel.yaml
```

Клонируем исходники uhd с репозитория Ettus нужной ветки, например, UHD-4.1 (можно любую последнюю):

```bash
git clone -b UHD-4.1 https://github.com/EttusResearch/uhd.git
Делаем сборку:
cd uhd/host/
mkdir build
cd build
cmake ../
make -j8
make test
```

Устанавливаем libuhd:

```bash
sudo make install
sudo ldconfig
```

Проверяем установленную версию UHD на хосте:

```bash
uhd_config_info --print-all
```

Должно быть примерно так:

```bash
ubuntu@pc-ubuntu:~/gnodeb_du_25050$ uhd_config_info --ver
UHD 4.1.0.5-114-ge7b0bf74
```

Для некоторых тестов требуется версия libboost_program_options.so.1.65.1, которой нет в репозиториях в последних версиях Ubuntu. Для UHD 4.1 её требуется установить отдельно:

```bash
cd /tmp
wget https://boostorg.jfrog.io/artifactory/main/release/1.65.1/source/boost_1_65_1.tar.gz
tar -xzvf boost_1_65_1.tar.gz
cd boost_1_65_1/
./bootstrap.sh
./b2
sudo ./b2 install
sudo ldconfig
```

Установка/обновление UHD на USRP c помощью mender:
На хосте, к которому подключен напрямую USRP, скачиваем uhd-образы:

```bash
sudo uhd_images_downloader -t mender -t n3xx --yes
```

Копируем их на USRP:

```bash
scp /usr/local/share/uhd/images/usrp_n3xx_fs.mender root@192.168.20.2:~/
```
Заходим на USRP:

```bash
ssh root@192.168.20.2
```

Устанавливаем загруженный артефакт для mender:

```bash
mender -f -rootfs /home/root/usrp_n3xx_fs.mender
```
Если будет ошибка, тогда:

```bash
mender -install /home/root/usrp_n3xx_fs.mender
```

После этого ребут
```bash
reboot
```

Затем нужно снова подключиться к USRP

```bash
ssh root@192.168.20.2
```
Наконец, применить установленный артефакт
```bash
mender -commit
```
Проверяем установленную версию на USRP:

```bash
cat /etc/mender/artifact_info
uhd_config_info --version
```

Должно быть примерно так:

```bash
root@ni-n3xx-322C62B:~# cat /etc/mender/artifact_info
artifact_name=v4.1.0.6-rc1_n3xx
root@ni-n3xx-322C62B:~# uhd_config_info --version
UHD 4.1.0.HEAD-0-gc3fdc401
```