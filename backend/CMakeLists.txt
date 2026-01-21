cmake_minimum_required(VERSION 3.16)
project(MapBackend LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../build/bin)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Network
    Gui
)

# 收集源文件
set(SOURCES
    main.cpp
    server.cpp
)

set(HEADERS
    server.h
)

# 添加共享的数据结构文件
set(SHARED_SOURCES
    ../src/data/marker.cpp
    ../src/data/mapsnapshot.cpp
)

set(SHARED_HEADERS
    ../src/data/marker.h
    ../src/data/mapsnapshot.h
)

# 创建可执行文件
add_executable(MapBackend ${SOURCES} ${HEADERS} ${SHARED_SOURCES} ${SHARED_HEADERS})

# 链接 Qt 库
target_link_libraries(MapBackend PRIVATE
    Qt6::Network
    Qt6::Gui
)

# 包含共享头文件目录
target_include_directories(MapBackend PRIVATE ${CMAKE_SOURCE_DIR}/..)
