# =============================================================================
# 编译器与编译标志
# 目标:与 makefile 的 COMPILE_FLAG 逐项一致,保证 cmake 产物功能正确。
# =============================================================================

set(CMAKE_C_COMPILER gcc)

# 源码根(绝对路径)
set(LEOS_ROOT ${CMAKE_SOURCE_DIR})

# 编译标志:逐项对齐 makefile COMPILE_FLAG(去掉 -c,cmake 会自动加)
# makefile: -g -m32 -std=c11 -fno-builtin -fno-stack-protector -fno-pic -fno-pie
#           -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
#           -Wno-error=implicit-function-declaration -DDEBUG_ENABLE=1
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} \
    -g -m32 -std=c11 -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
    -Wno-error=int-conversion -Wno-error=incompatible-pointer-types \
    -Wno-error=implicit-function-declaration -DDEBUG_ENABLE=1")

# .S 文件交给 C 编译器预处理(cmake 默认用 as,不支持 #include/宏)。
# 在 leos_sources.cmake 收集完 LEOS_ASM_SRCS 后再 set LANGUAGE C。