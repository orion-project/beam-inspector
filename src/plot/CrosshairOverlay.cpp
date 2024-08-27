#include "CrosshairOverlay.h"

#include "app/HelpSystem.h"

#include "helpers/OriDialogs.h"

#include "qcp/src/core.h"
#include "qcp/src/painter.h"

#include <QColorDialog>

#define RADIUS 5
#define EXTENT 5
#define TEXT_POS 15

void Crosshair::setLabel(const QString &s)
{
    label = s;
    text = QStaticText(label);
    text.prepare();
}

CrosshairsOverlay::CrosshairsOverlay(QCustomPlot *parentPlot) : QCPAbstractItem(parentPlot)
{
    connect(parentPlot, &QCustomPlot::mouseMove, this, &CrosshairsOverlay::mouseMove);
    connect(parentPlot, &QCustomPlot::mousePress, this, &CrosshairsOverlay::mousePress);
    connect(parentPlot, &QCustomPlot::mouseRelease, this, &CrosshairsOverlay::mouseRelease);
}

void CrosshairsOverlay::setImageSize(int sensorW, int sensorH, const PixelScale &scale)
{
    _scale = scale;
    _maxPixelX = sensorW;
    _maxPixelY = sensorH;
    _maxUnitX = scale.pixelToUnit(sensorW);
    _maxUnitY = scale.pixelToUnit(sensorH);
    updateCoords();
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
        painter->setPen(c.color);
        const double x = parentPlot()->xAxis->coordToPixel(c.unitX);
        const double y = parentPlot()->yAxis->coordToPixel(c.unitY);
        painter->drawEllipse(QPointF(x, y), r, r);
        painter->drawLine(QLineF(x, y-w, x, y+w));
        painter->drawLine(QLineF(x-w, y, x+w, y));
        painter->setBrush(c.color);
        painter->drawStaticText(x+TEXT_POS, y-c.text.size().height()/2, c.text);
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
    setItemCoords(c, _selectedPos.x(), _selectedPos.y());
    _items << c;
    parentPlot()->replot();
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
        _items << c;
    }
    updateCoords();
    return {};
}
