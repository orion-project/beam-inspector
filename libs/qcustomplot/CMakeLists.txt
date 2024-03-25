add_library(qcustomplot STATIC
    qcustomplot.cpp
)

target_link_libraries(qcustomplot PRIVATE
    Qt::Widgets
    Qt::PrintSupport
)

target_include_directories(qcustomplot INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
