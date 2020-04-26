cmake_minimum_required(VERSION 3.10)

project(cfenc VERSION 0.2)

set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE RELEASE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

if (UNIX AND NOT APPLE)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    find_package(UUID REQUIRED)
endif (UNIX AND NOT APPLE)

find_package(FFMPEG REQUIRED)
find_package(CFHD REQUIRED)

add_executable(cfenc ${PROJECT_SOURCE_DIR}/cfenc.cpp)
configure_file(version.h.in "${PROJECT_SOURCE_DIR}/version.h")

set(CMAKE_INSTALL_PREFIX /usr/local/bin)
install(TARGETS cfenc DESTINATION ${CMAKE_INSTALL_PREFIX})

target_include_directories(cfenc PRIVATE
    "${FFMPEG_INCLUDE_DIR}"
    "${CFHD_INCLUDE_DIR}"
)

target_link_libraries(cfenc ${FFMPEG_LIBRARIES})
target_link_libraries(cfenc ${CFHD_LIBRARY})

if (UNIX AND NOT APPLE)
    target_link_libraries(cfenc Threads::Threads)
    target_link_libraries(cfenc ${UUID_LIBRARY})
endif (UNIX AND NOT APPLE)

MESSAGE("FFMPEG_INCLUDE_DIR=${FFMPEG_INCLUDE_DIR}")
MESSAGE("CFHD_INCLUDE_DIR=${CFHD_INCLUDE_DIR}")
MESSAGE("FFMPEG_LIBRARIES=${FFMPEG_LIBRARIES}")
MESSAGE("CFHD_LIBRARY=${CFHD_LIBRARY}")
