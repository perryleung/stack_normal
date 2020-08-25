#SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
#SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

INCLUDE_DIRECTORIES(include)

aux_source_directory(. SRC_LIST)

add_subdirectory(dev)
add_subdirectory(phy)
add_subdirectory(mac)
add_subdirectory(net)
add_subdirectory(tra)
add_subdirectory(core)

add_executable(demo ${SRC_LIST})

# 最低CMake版本要求
cmake_minimum_required(VERSION 3.10.0)

# 项目名称
project(stack_dif)

# 设置C/C++标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件路径
include_directories("include")

TARGET_LINK_LIBRARIES(demo dev phy mac net tra core)
