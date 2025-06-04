cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Inclui o Pico SDK
include(pico_sdk_import.cmake)

# Define o nome do projeto
project(smart_home_panel C CXX ASM)
pico_sdk_init()

# Inclui diretórios para bibliotecas adicionais
include_directories(${CMAKE_SOURCE_DIR}/lib)

# Adiciona o executável com os arquivos fonte
add_executable(${PROJECT_NAME}
    main.c
    lib/ssd1306.c
    ws2812.pio
)

# Gera o cabeçalho para o PIO
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Inclui diretórios para cabeçalhos
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/lwip/src/include
    ${PICO_SDK_PATH}/lib/lwip/src/include/arch
    ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
)

# Liga as bibliotecas necessárias
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_gpio
    hardware_i2c
    hardware_adc
    hardware_pio
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt  # Biblioteca MQTT do LWIP
)

# Habilita saída USB e desabilita UART
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Gera arquivos adicionais (uf2, hex, etc.)
pico_add_extra_outputs(${PROJECT_NAME})