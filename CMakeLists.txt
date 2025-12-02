cmake_minimum_required(VERSION 3.15)

# 1. Actualizamos el nombre del proyecto
project(strangerEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Salida en la carpeta bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(SOURCE_FILES src/main.cpp)

# 2. Actualizamos el nombre del ejecutable
add_executable(strangerEngine ${SOURCE_FILES})

# 3. Linkeamos las librerías necesarias para Windows
if(WIN32)
    target_link_libraries(strangerEngine PRIVATE user32 gdi32 winmm)
endif()