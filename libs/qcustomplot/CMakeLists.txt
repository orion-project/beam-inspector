add_library(qcustomplot STATIC
    colorgradient.cpp
    core.cpp
    global.h
    item.cpp
    layer.cpp
    layout.cpp
    lineending.cpp
    paintbuffer.cpp
    painter.cpp
    plottable.cpp
    scatterstyle.cpp
    selection.cpp
    #selectiondecorator-bracket.cpp
    selectionrect.cpp
    vector2d.cpp

    axis/axis.cpp
    axis/axisticker.cpp
    #axis/axistickerdatetime.cpp
    axis/axistickerfixed.cpp
    #axis/axistickerlog.cpp
    #axis/axistickerpi.cpp
    #axis/axistickertext.cpp
    #axis/axistickertime.cpp
    axis/labelpainter.cpp
    axis/range.cpp

    #items/item-bracket.cpp
    #items/item-curve.cpp
    items/item-ellipse.cpp
    #items/item-line.cpp
    #items/item-pixmap.cpp
    #items/item-rect.cpp
    items/item-straightline.cpp
    items/item-text.cpp
    #items/item-tracer.cpp

    layoutelements/layoutelement-axisrect.cpp
    layoutelements/layoutelement-colorscale.cpp
    layoutelements/layoutelement-legend.cpp
    layoutelements/layoutelement-textelement.cpp

    #plottables/plottable-bars.cpp
    plottables/plottable-colormap.cpp
    #plottables/plottable-curve.cpp
    #plottables/plottable-errorbar.cpp
    #plottables/plottable-financial.cpp
    plottables/plottable-graph.cpp
    #plottables/plottable-statisticalbox.cpp

    #polar/layoutelement-angularaxis.cpp
    #polar/polargraph.cpp
    #polar/polargrid.cpp
    #polar/radialaxis.cpp
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

target_include_directories(qcustomplot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(qcustomplot INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
