# Host-side unit tests for the EgoRAM firmware. Build with a native compiler,
# not the ARM cross toolchain; GCC >= 13 or Clang >= 17 is required for the
# C23 "enum : uint8_t" syntax used by the sources.
#
# With the clang from Visual Studio (and the cmake/ninja from the pico SDK),
# run from the repository root:
#
#   set LLVM=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin
#   cmake -S test -B test/build -G Ninja -DCMAKE_C_COMPILER="%LLVM%/clang.exe" -DCMAKE_RC_COMPILER="%LLVM%/llvm-rc.exe"
#   cmake --build test/build
#   ctest --test-dir test/build
#
# The pico_shim folder stands in for the pico SDK headers, so the device
# compile units build unmodified on the host.

cmake_minimum_required(VERSION 3.13)
project(EgoRAMTests C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for IDE IntelliSense

enable_testing()

add_executable(test_ego_mos6502_device
        test_ego_mos6502_device.c
        EgoRAM_stubs.c
        ../src/ego_mos6502_device.c
        ../src/fake6502.c
)
target_include_directories(test_ego_mos6502_device PRIVATE ../src pico_shim)
add_test(NAME ego_mos6502_device COMMAND test_ego_mos6502_device)
