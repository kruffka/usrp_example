# Пример с RX и TX одновременно
add_executable(usrp_example usrp_main.c)
target_link_libraries(usrp_example common usrplib)

# Пример с чтением с USRP и записью результата в файл
add_executable(usrp_rx_to_file usrp_rx_to_file.c)
target_link_libraries(usrp_rx_to_file common usrplib)

# Пример с записью в USRP сэмплов из файла
add_executable(usrp_tx_from_file usrp_tx_from_file.c)
target_link_libraries(usrp_tx_from_file common usrplib)