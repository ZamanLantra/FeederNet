cmake_minimum_required(VERSION 3.10)
project(FeedernetTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

add_subdirectory(test)