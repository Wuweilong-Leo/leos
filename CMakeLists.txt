cmake_minimum_required(VERSION 3.16)
project(leos LANGUAGES C)

# 模块化:编译器/标志、源文件收集、链接/目标分文件维护,
# 与 makefile 保持逐项对齐以保证两套构建产物二进制一致。
# 详见各模块顶部注释。
include(${CMAKE_SOURCE_DIR}/cmake/leos_compiler.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/leos_sources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/leos_link.cmake)