# Rumble: an OpenRAC native mod for Ratchet & Clank (from the SDK's template).
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build && ctest --test-dir build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rumble
#
# A tag v<version> (the manifest's) makes the workflow publish the release the launcher installs.
cmake_minimum_required(VERSION 3.20)
project(rumble LANGUAGES C CXX)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
# set_motors, the pads' motors: interface 1.37.
find_package(OpenRACMod 1.37 REQUIRED)

# Windows: GitHub's windows-2025 image pairs a Visual Studio whose C++ library asks for a newer Clang
# than the clang-cl it ships (STL1000), depending on the image a run gets. The library's own switch
# lets this clang-cl build against it; MSVC is true for clang-cl too.
if(MSVC)
  add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()

# Ratchet & Clank's vibration mixer, copied from rac1-decomp (THIRD_PARTY_NOTICES.md).
set(mixer src/actuator.c)

openrac_add_mod(rumble MANIFEST openrac-mod.json SOURCES src/mod.cpp ${mixer})
target_compile_features(rumble PRIVATE cxx_std_17)

# The notices of what is copied go in the package (and so the archive, which carries the files
# named LICENSE* beside the manifest).
get_target_property(rumble_package rumble OPENRAC_MOD_PACKAGE_DIR)
configure_file(THIRD_PARTY_NOTICES.md "${rumble_package}/LICENSE.THIRD_PARTY_NOTICES.md" COPYONLY)

# The rumble's rules and the game reader, with the copied mixer, on a fake game (ctest; the
# workflow runs it on every platform).
enable_testing()
add_executable(rumble_test src/rumble_test.cpp ${mixer})
target_compile_features(rumble_test PRIVATE cxx_std_17)
add_test(NAME rumble COMMAND rumble_test)
