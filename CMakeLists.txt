cmake_minimum_required(VERSION 3.23)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
project(LBA2MenuProto C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 98)
if(EMSCRIPTEN)
    # em++ defaults to C++17, so CMake would leave out -std=gnu++98 and build the
    # 1997 code as C++17; as the desktop compilers, say the standard every time.
    # Targets set to C++17 add their own -std after this one, which wins.
    string(APPEND CMAKE_CXX_FLAGS " -std=gnu++98")
endif()

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

# sdl3 on a desktop; uwp for an Xbox in Developer Mode, with lba2cc's UWP
# toolchain (tools/build-xbox.sh).
if(PLATFORM_BACKEND STREQUAL "sdl3")
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
elseif(NOT PLATFORM_BACKEND STREQUAL "uwp")
    message(FATAL_ERROR "The prototype builds with PLATFORM_BACKEND sdl3 or uwp, not ${PLATFORM_BACKEND}")
endif()
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

# Zilla Slab Bold (SIL OFL, assets/fonts/OFL.txt) baked into the binary for the Font row.
set(EMBEDDED_FONTS_CPP)
foreach(_face Bold)
    set(_out ${CMAKE_BINARY_DIR}/embedded_font_${_face}.cpp)
    add_custom_command(
        OUTPUT ${_out}
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/ZillaSlab-${_face}.ttf"
                "-DOUTPUT=${_out}" "-DSYMBOL=g_zillaSlab${_face}Ttf" -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/ZillaSlab-${_face}.ttf ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake
        COMMENT "Embedding ZillaSlab-${_face}.ttf")
    list(APPEND EMBEDDED_FONTS_CPP ${_out})
endforeach()

add_executable(lba2menu
    src/main.cpp
    src/engine.cpp
    src/cadre.cpp
    src/menu.cpp
    src/controls.cpp
    src/experimental.cpp
    src/ui.cpp
    src/trace.cpp
    src/shot.cpp
    src/screenbg.cpp
    src/vfont.cpp
    src/twinsun.cpp
    src/lang.cpp
    ${EMBEDDED_FONTS_CPP}
    src/selftest.cpp
    ${LBA2CC_GAME_SOURCES}
    $<TARGET_OBJECTS:lba2_platform_entry>
)
target_include_directories(lba2menu PRIVATE
    src
    third_party
    ${LBA2CC_DIR}/LIB386/H
    ${LBA2CC_DIR}/SOURCES)
target_link_libraries(lba2menu PRIVATE svg sys poly 3D lba2_platform)
if(PLATFORM_BACKEND STREQUAL "sdl3")
    # SDL only for the desktop window's size and title; input goes through lba2cc's platform layer.
    target_link_libraries(lba2menu PRIVATE SDL3::SDL3)
    target_compile_definitions(lba2menu PRIVATE LBA2_PROTO_SDL=1)
else()
    # As lba2cc's SOURCES/CMakeLists.txt links its UWP exe: the C++ runtime inside,
    # the entry's wWinMain, no console, and the entry's exit wrapper.
    target_link_options(lba2menu PRIVATE -static -municode -mwindows -Wl,--wrap=exit)
endif()
set_target_properties(lba2menu PROPERTIES CXX_STANDARD 17)
if(EMSCRIPTEN)
    target_sources(lba2menu PRIVATE src/web.cpp)
    # The web build: the page in web/ asks for the game files and starts the menu (tools/build-web.sh).
    target_link_options(lba2menu PRIVATE
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=67108864
        -sFORCE_FILESYSTEM -sINVOKE_RUN=0 -sEXIT_RUNTIME
        -sEXPORTED_RUNTIME_METHODS=FS,ENV,callMain
        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
    set_target_properties(lba2menu PROPERTIES SUFFIX ".html"
        LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
endif()
