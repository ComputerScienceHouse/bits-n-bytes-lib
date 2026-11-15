if(ESP_PLATFORM)
    idf_component_register(SRCS "bnb_uart.c" "port_esp32.c"
                           INCLUDE_DIRS "."
                           REQUIRES driver)
else()
    cmake_minimum_required(VERSION 3.10)
    project(bnb_uart C)
    add_library(bnb_uart SHARED bnb_uart.c port_linux.c)
    target_include_directories(bnb_uart PUBLIC .)
endif()
