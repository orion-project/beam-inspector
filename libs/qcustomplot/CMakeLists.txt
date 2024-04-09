set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(qcustomplot STATIC
    qcp/src/colorgradient.cpp
    qcp/src/core.cpp
    qcp/src/global.h
    qcp/src/item.cpp
    qcp/src/layer.cpp
    qcp/src/layout.cpp
    qcp/src/lineending.cpp
    qcp/src/paintbuffer.cpp
    qcp/src/painter.cpp
    qcp/src/plottable.cpp
    qcp/src/scatterstyle.cpp
    qcp/src/selection.cpp
    #qcp/src/selectiondecorator-bracket.cpp
    qcp/src/selectionrect.cpp
    qcp/src/vector2d.cpp

    qcp/src/axis/axis.cpp
    qcp/src/axis/axisticker.cpp
    #qcp/src/axis/axistickerdatetime.cpp
    qcp/src/axis/axistickerfixed.cpp
    #qcp/src/axis/axistickerlog.cpp
    #qcp/src/axis/axistickerpi.cpp
    #qcp/src/axis/axistickertext.cpp
    #qcp/src/axis/axistickertime.cpp
    qcp/src/axis/labelpainter.cpp
    qcp/src/axis/range.cpp

    #qcp/src/items/item-bracket.cpp
    #qcp/src/items/item-curve.cpp
    qcp/src/items/item-ellipse.cpp
    #qcp/src/items/item-line.cpp
    #qcp/src/items/item-pixmap.cpp
    #qcp/src/items/item-rect.cpp
    qcp/src/items/item-straightline.cpp
    qcp/src/items/item-text.cpp
    #qcp/src/items/item-tracer.cpp

    qcp/src/layoutelements/layoutelement-axisrect.cpp
    qcp/src/layoutelements/layoutelement-colorscale.cpp
    qcp/src/layoutelements/layoutelement-legend.cpp
    qcp/src/layoutelements/layoutelement-textelement.cpp

    #qcp/src/plottables/plottable-bars.cpp
    qcp/src/plottables/plottable-colormap.cpp
    #qcp/src/plottables/plottable-curve.cpp
    #qcp/src/plottables/plottable-errorbar.cpp
    #qcp/src/plottables/plottable-financial.cpp
    qcp/src/plottables/plottable-graph.cpp
    #qcp/src/plottables/plottable-statisticalbox.cpp

    #qcp/src/polar/layoutelement-angularaxis.cpp
    #qcp/src/polar/polargraph.cpp
    #qcp/src/polar/polargrid.cpp
    #qcp/src/polar/radialaxis.cpp
)

target_link_libraries(qcustomplot PRIVATE
    Qt::Widgets
    Qt::PrintSupport
)

target_include_directories(qcustomplot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qcp/src
)

target_include_directories(qcustomplot INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
