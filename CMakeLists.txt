# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Project details.
set(PROJECT JubileeWind)
set(MAJOR_VERSION 0)
set(MINOR_VERSION 1)

# Directories.
set(BUILD "${CMAKE_SOURCE_DIR}/build")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Declare project.
project(${PROJECT} C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Make project data accessible to compiler
add_compile_definitions(PROJECT=${PROJECT})
add_compile_definitions(MAJOR_VERSION=${MAJOR_VERSION})
add_compile_definitions(MINOR_VERSION=${MINOR_VERSION})

# Create executable.
add_executable(${PROJECT})

# Specify sources for the executable.
target_sources(${PROJECT} PRIVATE
    src/${PROJECT}.cpp
)

# Specify link libraries.
target_link_libraries(
    ${PROJECT}
    pico_stdlib
    etl::etl
)

# Enable stdio outputs as required.
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 1)

# Create additional executables.
pico_add_extra_outputs(${PROJECT})

# Add required subdirectories for the libraries required.
add_subdirectory(external/json)
message("json library added.")
add_subdirectory(external/etl)
message("etl library added.")






