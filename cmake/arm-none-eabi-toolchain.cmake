# cmake/arm-none-eabi-toolchain.cmake
# CMake toolchain file for STM32F103 (Cortex-M3) using xPack GCC
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake ..

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# ---- Toolchain path (adjust if GCC is on PATH) ----
if(WIN32)
  set(GCC_BIN_DIR "C:/Users/Administrator.DESKTOP-T0UF4T8/gcc-arm/xpack-arm-none-eabi-gcc-13.2.1-1.1/bin")
  set(EXE_SUFFIX ".exe")
else()
  set(GCC_BIN_DIR "")
  set(EXE_SUFFIX "")
endif()

set(CMAKE_C_COMPILER   "${GCC_BIN_DIR}/arm-none-eabi-gcc${EXE_SUFFIX}"  CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${GCC_BIN_DIR}/arm-none-eabi-g++${EXE_SUFFIX}"  CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${GCC_BIN_DIR}/arm-none-eabi-gcc${EXE_SUFFIX}"  CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY      "${GCC_BIN_DIR}/arm-none-eabi-objcopy${EXE_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_SIZE_UTIL    "${GCC_BIN_DIR}/arm-none-eabi-size${EXE_SUFFIX}"  CACHE FILEPATH "" FORCE)

# Prevent CMake from testing the compiler with a host link step
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Skip compiler tests that require running executables
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

# Cortex-M3 flags
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")

set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS} -x assembler-with-cpp")

# Search only in cross-compilation sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
