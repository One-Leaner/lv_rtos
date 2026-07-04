cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME lv_rtos)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# 移除工具链文件中的链接脚本设置
string(REPLACE "-T \"${CMAKE_SOURCE_DIR}/STM32F407VGTx_FLASH.ld\"" "" CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS}")
string(REPLACE "-T \"${CMAKE_SOURCE_DIR}/STM32F407VGTx_FLASH.ld\"" "" CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(application)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# 添加lvgl库
add_subdirectory(${CMAKE_SOURCE_DIR}/code/application/lib/lvgl)

# 添加freetype库
add_subdirectory(${CMAKE_SOURCE_DIR}/code/application/lib/freetype-2.14.3)

# Link directories setup
target_link_directories(application PRIVATE

    # Add user defined library search paths
)

file(GLOB APP_SOURCES ${CMAKE_SOURCE_DIR}/code/application/app/src/*.c)
file(GLOB BSP_SOURCES ${CMAKE_SOURCE_DIR}/code/application/bsp/src/*.c)

# Add sources to executable
target_sources(application PRIVATE

    ${APP_SOURCES}
    ${BSP_SOURCES}

    ${CMAKE_SOURCE_DIR}/code/application/lib/InfoNES/src/InfoNES_Mapper.cpp
    ${CMAKE_SOURCE_DIR}/code/application/lib/InfoNES/src/InfoNES_System.c
    ${CMAKE_SOURCE_DIR}/code/application/lib/InfoNES/src/K6502.cpp
    ${CMAKE_SOURCE_DIR}/code/application/lib/InfoNES/src/InfoNES.cpp

    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_cpu.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_default.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_ppu.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_rom.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/port/nes_port.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper/nes_mapper0.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper/nes_mapper1.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper/nes_mapper2.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper/nes_mapper3.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper/nes_mapper4.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/src/nes_mapper/nes_mapper7.c

    # ${CMAKE_SOURCE_DIR}/code/application/lib/cm_backtrace/cm_backtrace.c
    # ${CMAKE_SOURCE_DIR}/code/application/lib/cm_backtrace/fault_handler/gcc/cmb_fault.S
)

# Add include paths
target_include_directories(application PRIVATE

    # Add user defined include paths
    ${CMAKE_SOURCE_DIR}/code/application/app/inc
    ${CMAKE_SOURCE_DIR}/code/application/bootloader/inc
    ${CMAKE_SOURCE_DIR}/code/application/bsp/inc
    ${CMAKE_SOURCE_DIR}/code/application/lib

    ${CMAKE_SOURCE_DIR}/code/application/lib/InfoNES/src

    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/inc
    # ${CMAKE_SOURCE_DIR}/code/application/lib/nes/port

    # ${CMAKE_SOURCE_DIR}/code/application/lib/cm_backtrace
)

# Add project symbols (macros)
target_compile_definitions(application PRIVATE

    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(application
    stm32cubemx
    lvgl

    # Add user defined libraries
)

target_link_libraries(lvgl PRIVATE
    freetype
)

# lvgl 编译选项
target_include_directories(lvgl PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
)

# 浮点数支持
set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS} -Wl,-u,_printf_float,-u,_scanf_float")

# 替换链接脚本
# string(REPLACE "STM32F407VGTx_FLASH.ld" "STM32F407VGTx_FLASH_app.ld" CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS}")
# string(REPLACE "STM32F407VGTx_FLASH.ld" "STM32F407VGTx_FLASH_app.ld" CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
set_target_properties(application PROPERTIES
    LINK_FLAGS "-T\"${CMAKE_SOURCE_DIR}/STM32F407VGTx_FLASH_app.ld\" -Wl,-Map=${CMAKE_BINARY_DIR}/application.map"
)

# ---------------------- 新增：自动生成 HEX + BIN 文件 ----------------------
# 1. 定义 bin 文件的名称和输出路径（和 elf 文件同目录）
set(BIN_FILE ${CMAKE_BINARY_DIR}/application.bin)

# 2. 自动生成 bin 文件（核心：关联 application 目标，POST_BUILD 执行转换）
add_custom_command(
    TARGET application # 关联你的项目可执行目标
    POST_BUILD # 执行时机：目标文件（elf）构建完成后
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:application> ${BIN_FILE}
    COMMENT "Automatically generating BIN file: ${BIN_FILE}" # 终端提示信息，可选保留
)

# 定义一个变量，指向反汇编文件
# set(DISASSEMBLY_FILE ${CMAKE_CURRENT_BINARY_DIR}/application.lst)

# # 添加一个自定义命令，在构建后自动执行
# add_custom_command(TARGET application
# POST_BUILD
# COMMAND ${CMAKE_OBJDUMP} -D -S -M force-thumb application.elf > ${DISASSEMBLY_FILE}
# COMMENT "Generating disassembly file (${DISASSEMBLY_FILE})"
# )

# Bootloader
add_executable(bootloader)

target_compile_definitions(bootloader PRIVATE
    USE_HAL_DRIVER
    STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>
)

# Bootloader sources
file(GLOB BOOTLOADER_SOURCES ${CMAKE_SOURCE_DIR}/code/bootloader/boot/src/*.c)
file(GLOB BOOTBSP_SOURCES ${CMAKE_SOURCE_DIR}/code/bootloader/bsp/src/*.c)

target_sources(bootloader PRIVATE
    ${BOOTLOADER_SOURCES}
    ${BOOTBSP_SOURCES}
    ${CMAKE_SOURCE_DIR}/startup_stm32f407xx_boot.s

    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/Core/Src/usart.c
    ${CMAKE_SOURCE_DIR}/Core/Src/spi.c
    ${CMAKE_SOURCE_DIR}/Core/Src/dma.c
    ${CMAKE_SOURCE_DIR}/Core/Src/gpio.c
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_hal_timebase_tim.c

    # HAL 核心文件
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c

    # 系统调用实现（Newlib C库必需）
    ${CMAKE_SOURCE_DIR}/Core/Src/syscalls.c
    ${CMAKE_SOURCE_DIR}/Core/Src/sysmem.c
)

target_include_directories(bootloader PRIVATE
    ${CMAKE_SOURCE_DIR}/code/bootloader/boot/inc
    ${CMAKE_SOURCE_DIR}/code/bootloader/bsp/inc

    ${CMAKE_SOURCE_DIR}/Core/Inc # STM32 核心头文件
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc # HAL 头文件
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include # CMSIS 头文件
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include # CMSIS 核心头文件
)

set_target_properties(bootloader PROPERTIES
    LINK_FLAGS "-T\"${CMAKE_SOURCE_DIR}/STM32F407VGTx_FLASH_boot.ld\" -Wl,-Map=${CMAKE_BINARY_DIR}/bootloader.map"
)

# 生成bootloader的bin文件
set(BOOTLOADER_BIN ${CMAKE_BINARY_DIR}/bootloader.bin)
add_custom_command(TARGET bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader> ${BOOTLOADER_BIN}
    COMMENT "Generating bootloader BIN: ${BOOTLOADER_BIN}"
)