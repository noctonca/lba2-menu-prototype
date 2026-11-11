# The web build's SDL3: Emscripten's own port (--use-port=sdl3), offered under
# the target names lba2cc's CMake asks find_package(SDL3) for.
foreach(_t SDL3::SDL3 SDL3::SDL3-shared SDL3::SDL3-static)
    if(NOT TARGET ${_t})
        add_library(${_t} INTERFACE IMPORTED)
        set_target_properties(${_t} PROPERTIES
            INTERFACE_COMPILE_OPTIONS "--use-port=sdl3"
            INTERFACE_LINK_OPTIONS "--use-port=sdl3")
    endif()
endforeach()
set(SDL3_FOUND TRUE)
set(SDL3_SDL3-shared_FOUND TRUE)
set(SDL3_SDL3-static_FOUND TRUE)
set(SDL3_VERSION 3.2.0)
