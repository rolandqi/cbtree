cmake_minimum_required(VERSION 2.8)
project(CBTree)

set(CBTree_VERSION_MAJOR 0)
set(CBTree_VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "-O -g -fsanitize=address -static-libasan -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${BUILD_ARCH}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(Threads REQUIRED)
# find_package (glog 0.3.3 REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

option(COVERALLS "Generate coveralls data" OFF)

if (COVERALLS)
    include(Coveralls)
    include(Utils)
    coveralls_turn_on_coverage()

    # Create the coveralls target.
    file(GLOB_RECURSE srcs ${PROJECT_SOURCE_DIR}/src/*.cpp)

    peloton_convert_absolute_paths(srcs)
    peloton_convert_absolute_paths(parser_srcs)

    #message(STATUS "Coverage srcs : ${srcs}" )
    set(COVERAGE_SRCS ${srcs})

    coveralls_setup("${COVERAGE_SRCS}" ON)
endif ()

include_directories(${CMAKE_SOURCE_DIR}/test)
include_directories(${CMAKE_SOURCE_DIR}/third_party)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src/Bolt)

file(GLOB SRC_BOLT ${PROJECT_SOURCE_DIR}/src/Bolt/*.cc)
file(GLOB INC_BOLT ${PROJECT_SOURCE_DIR}/src/Bolt/*.h)

set(SRC
        ${SRC_BOLT}
        ${INC_BOLT}
        )
add_library(bolt ${SRC})
add_subdirectory(test)