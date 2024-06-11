add_compile_options(
    -O3
    -ffast-math
    -funsafe-math-optimizations
    -msse4.2
)

add_library(cgn_beam_render STATIC
    beam_render.h beam_render.c
)

target_include_directories(cgn_beam_render INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
