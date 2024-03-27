add_library(qcustomplot STATIC
    qcustomplot.cpp
)

#add_compile_options(
#    -O3
#    -ffast-math
#    -funsafe-math-optimizations
#    -msse4.2
#)

target_link_libraries(qcustomplot PRIVATE
    Qt::Widgets
    Qt::PrintSupport
)

target_include_directories(qcustomplot INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
