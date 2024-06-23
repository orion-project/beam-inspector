#include "CrosshairOverlay.h"

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
    _maxX = scale.pixelToUnit(sensorW);
    _maxY = scale.pixelToUnit(sensorH);
    updateCoords();
}

void CrosshairsOverlay::updateCoords()
{
    for (auto& c : _items) {
        c.unitX = _scale.pixelToUnit(c.pixelX);
        c.unitY = _scale.pixelToUnit(c.pixelY);
    }
}

void CrosshairsOverlay::draw(QCPPainter *painter)
{
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
    c.unitX = parentPlot()->xAxis->pixelToCoord(x+dx);
    c.unitY = parentPlot()->yAxis->pixelToCoord(y+dy);
    c.pixelX = _scale.unitToPixel(c.unitX);
    c.pixelY = _scale.unitToPixel(c.unitY);
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
    auto label = Ori::Dlg::inputText(qApp->tr("Crosshair label"), c.label, &ok);
    if (ok) {
        c.setLabel(label);
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
    c.unitX = parentPlot()->xAxis->pixelToCoord(_selectedPos.x());
    c.unitY = parentPlot()->yAxis->pixelToCoord(_selectedPos.y());
    c.pixelX = _scale.unitToPixel(c.unitX);
    c.pixelY = _scale.unitToPixel(c.unitY);
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
        s->setValue("x", c.pixelX);
        s->setValue("y", c.pixelY);
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
        c.pixelX = s->value("x").toInt();
        c.pixelY = s->value("y").toInt();
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
            {"x", c.pixelX},
            {"y", c.pixelY},
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
        c.pixelX = item["x"].toInt();
        c.pixelY = item["y"].toInt();
        c.setLabel(item["label"].toString());
        c.color = QColor(item["color"].toString());
        _items << c;
    }
    updateCoords();
    return {};
}
