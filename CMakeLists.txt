cmake_minimum_required(VERSION 3.10)
project(FileTransfer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(file_transfer_lib
    src/ThreadPool.cpp
    src/FileTransfer.cpp
)

add_executable(server src/Server.cpp)
add_executable(client src/Client.cpp)

target_link_libraries(server file_transfer_lib pthread)
target_link_libraries(client file_transfer_lib pthread) 