cmake_minimum_required(VERSION 4.2.0) # cmake 版本
project(kvserver VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20) # C++ 版本
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加可执行文件
add_executable(kvserver src/main.cpp)

add_executable(kvserver_tests
    src/server/resp.cpp
    src/server/store.cpp

    tests/test_main.cpp
    tests/test_more.cpp
    tests/test_resp.cpp
    tests/test_store.cpp
)

add_executable(kvserver_server
    src/server/resp.cpp

    src/server/server.cpp
)

# 添加 include 目录
target_include_directories(kvserver_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(kvserver_server PRIVATE ${PROJECT_SOURCE_DIR}/include)