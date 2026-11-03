cmake_minimum_required(VERSION 3.23)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
project(LBA2MenuProto C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 98)

# lba2cc's LIB386 is built from the submodule, with its own CMake files.
# Its options are set here the way lba2cc's root CMakeLists sets them.
set(LBA2CC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lba2cc)
set(PLATFORM_BACKEND "sdl3" CACHE STRING "lba2cc platform backend")
set(SOUND_BACKEND "null" CACHE STRING "lba2cc sound backend")
set(MVIDEO_BACKEND "null" CACHE STRING "lba2cc motion video backend")
set(LBA2_BUILD_TESTS OFF)
set(ENABLE_POLY_RECORDING OFF)
set(ENABLE_ASM OFF)

if(NOT MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-fno-strict-aliasing>)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-ffp-contract=off>)
endif()
if(APPLE)
    add_link_options(-Wl,-no_warn_duplicate_libraries)
endif()

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
add_subdirectory(${LBA2CC_DIR}/LIB386/PLATFORM lib386_platform)
# lba2cc's PLATFORM CMake names its headers through CMAKE_SOURCE_DIR, which is
# this repository here; point the targets at the submodule's headers.
foreach(t lba2_platform lba2_platform_entry)
    target_include_directories(${t} PUBLIC ${LBA2CC_DIR}/LIB386/H)
endforeach()
add_subdirectory(${LBA2CC_DIR}/LIB386 lib386)

# Game-layer files used as they are, straight from the submodule.
set(LBA2CC_GAME_SOURCES
    ${LBA2CC_DIR}/SOURCES/PLASMA.CPP
    ${LBA2CC_DIR}/SOURCES/FIRE.CPP
    ${LBA2CC_DIR}/SOURCES/ANIMTEX.CPP
)

add_executable(lba2menu
    src/main.cpp
    src/engine.cpp
    src/cadre.cpp
    src/menu.cpp
    src/controls.cpp
    src/ui.cpp
    src/trace.cpp
    src/shot.cpp
    ${LBA2CC_GAME_SOURCES}
)
target_include_directories(lba2menu PRIVATE
    src
    ${LBA2CC_DIR}/LIB386/H
    ${LBA2CC_DIR}/SOURCES)
target_link_libraries(lba2menu PRIVATE svg sys poly 3D lba2_platform SDL3::SDL3)
set_target_properties(lba2menu PROPERTIES CXX_STANDARD 17)
