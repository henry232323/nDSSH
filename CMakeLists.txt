cmake_minimum_required(VERSION 3.15)
project(nDS_Projects C)

set(CMAKE_C_STANDARD 99)
INCLUDE_DIRECTORIES(/opt/devkitpro/libnds/include/)
LINK_DIRECTORIES(/opt/devkitpro/libnds/)

add_executable(nDS_Projects source/main.c)

