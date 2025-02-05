#include "CrosshairOverlay.h"

#include "app/HelpSystem.h"

#include "helpers/OriDialogs.h"

#include "qcustomplot/src/core.h"
#include "qcustomplot/src/painter.h"

#include <QColorDialog>

#define RADIUS 5
#define EXTENT 5
#define TEXT_POS 15
#define DEF_ROI_SIZE 0.1
#define DEF_DELTA_MIN 0.005
#define DEF_DELTA_MAX 0.05

void Crosshair::setLabel(const QString &s)
{
    label = s;
    text = QStaticText(label);
    text.prepare();
}

CrosshairsOverlay::CrosshairsOverlay(QCustomPlot *parentPlot) : BeamPlotItem(parentPlot)
{
    connect(parentPlot, &QCustomPlot::mouseMove, this, &CrosshairsOverlay::mouseMove);
    connect(parentPlot, &QCustomPlot::mousePress, this, &CrosshairsOverlay::mousePress);
    connect(parentPlot, &QCustomPlot::mouseRelease, this, &CrosshairsOverlay::mouseRelease);
}

void CrosshairsOverlay::updateCoords()
{
    for (auto& c : _items) {
        // Before 0.0.14 coords were saved as pixels
        if (c.x > 1) {
            if (c.x > _maxPixelX)
                c.x = _maxPixelX;
            c.x /= _maxPixelX;
        } else if (c.x < 0) {
            c.x = 0;
        }
        if (c.y > 1) {
            if (c.y > _maxPixelY)
                c.y = _maxPixelY;
            c.y /= _maxPixelY;
        } else if (c.y < 0) {
            c.y = 0;
        }
        c.pixelX = _maxPixelX * c.x;
        c.pixelY = _maxPixelY * c.y;
        c.unitX = _scale.pixelToUnit(c.pixelX);
        c.unitY = _scale.pixelToUnit(c.pixelY);
    }
    _hasCoords = true;
}

void CrosshairsOverlay::setItemCoords(Crosshair &c, double plotX, double plotY)
{
    c.unitX = parentPlot()->xAxis->pixelToCoord(plotX);
    c.unitY = parentPlot()->yAxis->pixelToCoord(plotY);
    c.pixelX = _scale.unitToPixel(c.unitX);
    c.pixelY = _scale.unitToPixel(c.unitY);
    c.x = c.pixelX / _maxPixelX;
    c.y = c.pixelY / _maxPixelY;
}

void CrosshairsOverlay::draw(QCPPainter *painter)
{
    if (!_hasCoords) return;
    const double r = RADIUS;
    const double w = RADIUS + EXTENT;
    for (const auto& c : _items) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(c.color, 0, Qt::SolidLine));
        const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
        const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
        painter->drawEllipse(QPointF(x, y), r, r);
        painter->drawLine(QLineF(x, y-w, x, y+w));
        painter->drawLine(QLineF(x-w, y, x+w, y));
        painter->setBrush(c.color);
        painter->drawStaticText(x+TEXT_POS, y-c.text.size().height()/2, c.text);
        painter->setPen(QPen(c.color, 0, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
    }
}

void CrosshairsOverlay::mousePress(QMouseEvent *e)
{
    if (!visible() || !_editing || e->button() != Qt::LeftButton) return;

    _draggingIdx = itemIndexAtPos(e->pos());
    if (_draggingIdx >= 0) {
        _dragX = e->pos().x();
        _dragY = e->pos().y();
    }
}

void CrosshairsOverlay::mouseRelease(QMouseEvent *e)
{
    if (_draggingIdx >= 0)
         if (onEdited)
            onEdited();
    _draggingIdx = -1;
}

void CrosshairsOverlay::mouseMove(QMouseEvent *e)
{
    if (_draggingIdx < 0) return;

    const int dx = e->pos().x() - _dragX;
    const int dy = e->pos().y() - _dragY;
    _dragX = e->pos().x();
    _dragY = e->pos().y();

    auto& c = _items[_draggingIdx];
    const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
    const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
    setItemCoords(c, x+dx, y+dy);
    parentPlot()->replot();
}

int CrosshairsOverlay::itemIndexAtPos(const QPoint& pos)
{
    const double r = RADIUS * 1.5;
    for (int i = 0; i < _items.size(); i++) {
        const auto& c = _items.at(i);
        const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
        const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
        int x1 = x-r, x2 = x+r, y1 = y-r, y2 = y+r;
        if (!c.label.isEmpty()) {
            x2 = x + TEXT_POS + c.text.size().width();
            int h = c.text.size().height();
            if (h > 2*r) {
                y1 = y - h/2;
                y2 = y + h/2;
            }
        }
        if (pos.x() >= x1 && pos.x() <= x2 && pos.y() >= y1 && pos.y() <= y2) {
            return i;
        }
    }
    return -1;
}

void CrosshairsOverlay::showContextMenu(const QPoint& pos)
{
    _selectedPos = pos;
    _selectedIdx = itemIndexAtPos(pos);
    QMenu *menu;
    if (_selectedIdx < 0) {
        if (!_menuForEmpty) {
            _menuForEmpty = new QMenu(parentPlot());
            _menuForEmpty->addAction(QIcon(":/toolbar/crosshair"), qApp->tr("Add crosshair"), this, &CrosshairsOverlay::addItem);
        }
        menu = _menuForEmpty;
    } else {
        if (!_menuForItem) {
            _menuForItem = new QMenu(parentPlot());
            _menuForItem->addAction(qApp->tr("Edit Label..."), this, &CrosshairsOverlay::changeItemLabel);
            _menuForItem->addAction(qApp->tr("Set Color..."), this, &CrosshairsOverlay::changeItemColor);
            _menuForItem->addSeparator();
            _menuForItem->addAction(QIcon(":/toolbar/trash"), qApp->tr("Remove"), this, &CrosshairsOverlay::removeItem);
        }
        menu = _menuForItem;
    }
    menu->popup(parentPlot()->mapToGlobal(pos));
}

void CrosshairsOverlay::changeItemColor()
{
    auto &c = _items[_selectedIdx];
    auto color = QColorDialog::getColor(c.color, parentPlot());
    if (color.isValid()) {
        c.color = color;
        parentPlot()->replot();
    }
}

void CrosshairsOverlay::changeItemLabel()
{
    auto &c = _items[_selectedIdx];
    bool ok;
    Ori::Dlg::InputTextOptions opts;
    opts.label = qApp->tr("Crosshair label");
    opts.value = c.label;
    opts.onHelp = [](){ HelpSystem::topic("overlays"); };
    if (Ori::Dlg::inputText(opts)) {
        c.setLabel(opts.value);
        parentPlot()->replot();
    }
}

void CrosshairsOverlay::removeItem()
{
    _items.removeAt(_selectedIdx);
    parentPlot()->replot();
    if (onEdited)
        onEdited();
}

void CrosshairsOverlay::addItem()
{
    int maxLabel = 0;
    for (const auto& c : _items) {
        bool ok;
        int label = c.label.toInt(&ok);
        if (ok && label > maxLabel)
            maxLabel = label;
    }
    Crosshair c;
    c.color = Qt::white;
    c.setLabel(QString::number(maxLabel+1));
    c.roiSize = DEF_ROI_SIZE;
    c.deltaMin = DEF_DELTA_MIN;
    c.deltaMax = DEF_DELTA_MAX;
    setItemCoords(c, _selectedPos.x(), _selectedPos.y());
    _items << c;
    parentPlot()->replot();
    if (onEdited)
        onEdited();
}

void CrosshairsOverlay::save(QSettings *s)
{
    s->beginGroup("Crosshairs");
    s->beginGroup("items");
    s->remove("");
    for (int i = 0; i < _items.size(); i++) {
        s->beginGroup(QString::number(i));
        const auto& c = _items.at(i);
        s->setValue("x", c.x);
        s->setValue("y", c.y);
        s->setValue("label", c.label);
        s->setValue("color", c.color.name());
        s->setValue("roiSize", c.roiSize);
        s->setValue("deltaMin", c.deltaMin);
        s->setValue("deltaMax", c.deltaMax);
        s->endGroup();
    }
    s->endGroup();
    s->endGroup();
}

void CrosshairsOverlay::load(QSettings *s)
{
    s->beginGroup("Crosshairs");
    s->beginGroup("items");
    for (const auto& g : s->childGroups()) {
        s->beginGroup(g);
        Crosshair c;
        c.x = s->value("x").toDouble();
        c.y = s->value("y").toDouble();
        c.setLabel(s->value("label").toString());
        c.color = QColor(s->value("color").toString());
        c.roiSize = s->value("roiSize").toDouble();
        c.deltaMin = s->value("deltaMin").toDouble();
        c.deltaMax = s->value("deltaMax").toDouble();
        _items << c;
        s->endGroup();
    }
    s->endGroup();
    s->endGroup();
}

void CrosshairsOverlay::clear()
{
    _items.clear();
}

QString CrosshairsOverlay::save(const QString& fileName)
{
    QJsonArray items;
    for (const auto &c : _items) {
        items << QJsonObject {
            {"x", c.x},
            {"y", c.y},
            {"label", c.label},
            {"color", c.color.name()},
            {"roi_size", c.roiSize},
            {"delta_min", c.deltaMin},
            {"delta_max", c.deltaMax}
        };
    }
    QJsonObject root;
    root["crosshairs"] = items;
    QJsonDocument doc(root);

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        qWarning() << "CrosshairsOverlay::save" << fileName << file.errorString();
        return file.errorString();
    }
    QTextStream stream(&file);
    stream << doc.toJson();
    file.close();
    return {};
}

QString CrosshairsOverlay::load(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "CrosshairsOverlay::load" << fileName << file.errorString();
        return file.errorString();
    }
    auto data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (doc.isNull())
        return error.errorString();
    QJsonObject root = doc.object();

    _items.clear();
    auto items = root["crosshairs"].toArray();
    for (auto it = items.begin(); it != items.end(); it++) {
        auto item = it->toObject();
        Crosshair c;
        c.x = item["x"].toDouble();
        c.y = item["y"].toDouble();
        c.setLabel(item["label"].toString());
        c.color = QColor(item["color"].toString());
        c.roiSize = item["roi_size"].toDouble();
        c.deltaMin = item["delta_min"].toDouble();
        c.deltaMax = item["delta_max"].toDouble();
        _items << c;
    }
    updateCoords();
    return {};
}

QList<RoiRect> CrosshairsOverlay::rois() const
{
    QList<RoiRect> res;
    for (const auto& it : _items) {
        double sz = (it.roiSize > 0 ? it.roiSize : DEF_ROI_SIZE) / 2.0;
        RoiRect r;
        r.left = it.x - sz;
        r.top = it.y - sz;
        r.right = it.x + sz;
        r.bottom = it.y + sz;
        res << r;
    }
    return res;
}

QList<QPointF> CrosshairsOverlay::positions() const
{
    QList<QPointF> res;
    for (const auto& it : _items)
        res << QPointF(it.x, it.y);
    return res;
}

QList<GoodnessLimits> CrosshairsOverlay::goodnessLimits(int expectedSize) const
{
    QList<GoodnessLimits> res;
    for (const auto& it : _items) {
        GoodnessLimits lim;
        lim.deltaMin = it.deltaMin > 0 ? it.deltaMin : DEF_DELTA_MIN;
        lim.deltaMax = it.deltaMax > 0 ? it.deltaMax : DEF_DELTA_MAX;
        if (lim.deltaMin > lim.deltaMax)
            std::swap(lim.deltaMin, lim.deltaMax);
        res << lim;
    }
    for (int i = res.size(); i < expectedSize; i++) {
        GoodnessLimits lim;
        lim.deltaMin = DEF_DELTA_MIN;
        lim.deltaMax = DEF_DELTA_MAX;
        res << lim;
    }
    return res;
}
