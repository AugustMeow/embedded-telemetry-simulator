set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 改成你的实际 bin 目录
set(HISI_TOOLCHAIN_DIR /opt/linux/x86-arm/aarch64-mix410-linux)

set(CMAKE_C_COMPILER   ${HISI_TOOLCHAIN_DIR}/bin/aarch64-mix410-linux-gcc)
set(CMAKE_CXX_COMPILER ${HISI_TOOLCHAIN_DIR}/bin/aarch64-mix410-linux-g++)
set(CMAKE_STRIP        ${HISI_TOOLCHAIN_DIR}/bin/aarch64-mix410-linux-strip)
set(CMAKE_AR           ${HISI_TOOLCHAIN_DIR}/bin/aarch64-mix410-linux-ar)

# 用 -print-sysroot 打印出来的路径填这里
set(CMAKE_SYSROOT /opt/linux/x86-arm/aarch64-mix410-linux/host_bin/../target)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# mix410 的 gcc 版本不算新，C++ 标准建议显式锁一下
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
