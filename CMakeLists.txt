cmake_minimum_required(VERSION 3.10)
project(steganography C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PNG REQUIRED)

add_executable(steg
    main.c
    encryption.c
    image_analysis.c
    embedding.c
)

target_link_libraries(steg
    pthread
    m
    PNG::PNG
)

