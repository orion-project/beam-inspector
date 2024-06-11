set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(qcustomplot STATIC
    qcp/src/colorgradient.h qcp/src/colorgradient.cpp
    qcp/src/core.h qcp/src/core.cpp
    qcp/src/global.h
    qcp/src/item.h qcp/src/item.cpp
    qcp/src/layer.h qcp/src/layer.cpp
    qcp/src/layout.h qcp/src/layout.cpp
    qcp/src/lineending.h qcp/src/lineending.cpp
    qcp/src/paintbuffer.h qcp/src/paintbuffer.cpp
    qcp/src/painter.h qcp/src/painter.cpp
    qcp/src/plottable.h qcp/src/plottable.cpp
    qcp/src/scatterstyle.h qcp/src/scatterstyle.cpp
    qcp/src/selection.h qcp/src/selection.cpp
    #qcp/src/selectiondecorator-bracket.h qcp/src/selectiondecorator-bracket.cpp
    qcp/src/selectionrect.h qcp/src/selectionrect.cpp
    qcp/src/vector2d.h qcp/src/vector2d.cpp

    qcp/src/axis/axis.h qcp/src/axis/axis.cpp
    qcp/src/axis/axisticker.h qcp/src/axis/axisticker.cpp
    #qcp/src/axis/axistickerdatetime.h qcp/src/axis/axistickerdatetime.cpp
    qcp/src/axis/axistickerfixed.h qcp/src/axis/axistickerfixed.cpp
    #qcp/src/axis/axistickerlog.h qcp/src/axis/axistickerlog.cpp
    #qcp/src/axis/axistickerpi.h qcp/src/axis/axistickerpi.cpp
    #qcp/src/axis/axistickertext.h qcp/src/axis/axistickertext.cpp
    #qcp/src/axis/axistickertime.h qcp/src/axis/axistickertime.cpp
    qcp/src/axis/labelpainter.h qcp/src/axis/labelpainter.cpp
    qcp/src/axis/range.h qcp/src/axis/range.cpp

    #qcp/src/items/item-bracket.h qcp/src/items/item-bracket.cpp
    #qcp/src/items/item-curve.h qcp/src/items/item-curve.cpp
    qcp/src/items/item-ellipse.h qcp/src/items/item-ellipse.cpp
    #qcp/src/items/item-line.h qcp/src/items/item-line.cpp
    #qcp/src/items/item-pixmap.h qcp/src/items/item-pixmap.cpp
    #qcp/src/items/item-rect.h qcp/src/items/item-rect.cpp
    qcp/src/items/item-straightline.h qcp/src/items/item-straightline.cpp
    qcp/src/items/item-text.h qcp/src/items/item-text.cpp
    #qcp/src/items/item-tracer.h qcp/src/items/item-tracer.cpp

    qcp/src/layoutelements/layoutelement-axisrect.h qcp/src/layoutelements/layoutelement-axisrect.cpp
    qcp/src/layoutelements/layoutelement-colorscale.h qcp/src/layoutelements/layoutelement-colorscale.cpp
    qcp/src/layoutelements/layoutelement-legend.h qcp/src/layoutelements/layoutelement-legend.cpp
    qcp/src/layoutelements/layoutelement-textelement.h qcp/src/layoutelements/layoutelement-textelement.cpp

    #qcp/src/plottables/plottable-bars.h qcp/src/plottables/plottable-bars.cpp
    qcp/src/plottables/plottable-colormap.h qcp/src/plottables/plottable-colormap.cpp
    #qcp/src/plottables/plottable-curve.h qcp/src/plottables/plottable-curve.cpp
    #qcp/src/plottables/plottable-errorbar.h qcp/src/plottables/plottable-errorbar.cpp
    #qcp/src/plottables/plottable-financial.h qcp/src/plottables/plottable-financial.cpp
    qcp/src/plottables/plottable-graph.h qcp/src/plottables/plottable-graph.cpp
    #qcp/src/plottables/plottable-statisticalbox.h qcp/src/plottables/plottable-statisticalbox.cpp

    #qcp/src/polar/layoutelement-angularaxis.h qcp/src/polar/layoutelement-angularaxis.cpp
    #qcp/src/polar/polargraph.h qcp/src/polar/polargraph.cpp
    #qcp/src/polar/polargrid.h qcp/src/polar/polargrid.cpp
    #qcp/src/polar/radialaxis.h qcp/src/polar/radialaxis.cpp
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
