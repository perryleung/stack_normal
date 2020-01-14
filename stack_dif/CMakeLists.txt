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

TARGET_LINK_LIBRARIES(demo dev phy mac net tra core)
