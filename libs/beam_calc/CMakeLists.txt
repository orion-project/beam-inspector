add_compile_options(
    -O3
    -ffast-math
    -funsafe-math-optimizations
    -msse4.2
)

add_library(cgn_beam_calc STATIC
    beam_calc.h beam_calc.c
)

#target_compile_definitions(cgn_beam_calc PRIVATE
#    USE_BLAS
#)

#target_include_directories(cgn_beam_calc PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}/../openblas/include
#)

#add_library(openblas STATIC IMPORTED GLOBAL)
#set_target_properties(openblas PROPERTIES
#    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../openblas/lib/libopenblas.a
#    IMPORTED_LOCATION_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/../openblas/lib/libopenblas.a
#    IMPORTED_LOCATION_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/../openblas/lib/libopenblas.a
#)

#target_link_libraries(cgn_beam_calc PUBLIC
#    openblas
#)

target_include_directories(cgn_beam_calc INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
