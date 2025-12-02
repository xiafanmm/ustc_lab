# Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
# Description: CMake构建脚本
# Author: User
# Create: 2023-12-01

cmake_minimum_required(VERSION 3.16)
project(Lab3_Chat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ----------------- 查找 Qt 库 -----------------
find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(Qt6_FOUND)
    message(STATUS "Found Qt6! Version: ${Qt6_VERSION}")
    set(QT_LIB Qt6::Widgets Qt6::Network)
else()
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 COMPONENTS Widgets Network REQUIRED)
    set(QT_LIB Qt5::Widgets Qt5::Network)
endif()

# 包含 common 目录
include_directories(common)

# ----------------- Server (纯 C++) -----------------
add_executable(chat_server 
    server/main.cpp 
)

if(UNIX)
    target_link_libraries(chat_server pthread)
endif()

# ----------------- Client (C++ + Qt) -----------------
add_executable(chat_client 
    client/main.cpp
    client/MainWindow.cpp
    client/MainWindow.h
)

target_link_libraries(chat_client ${QT_LIB})