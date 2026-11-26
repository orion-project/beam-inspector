#include "CamPropPresets.h"

#include "app/HelpSystem.h"
#include "cameras/CamPropControls.h"

#include "widgets/OriPopupMessage.h"

#include "helpers/OriDialogs.h"

#include <QAction>
#include <QGroupBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

using Self = CamPropPresets;

#define PRESET_NAME_KEY QStringLiteral("name")
#define BUTTON_HEIGHT 24

CamPropPresets::CamPropPresets(const char* logId, QObject* parent) : QObject(parent), _logId(logId)
{
    _groupBox = new QGroupBox(tr("Presets"));
    _groupBox->setLayout(new QVBoxLayout);
}

void CamPropPresets::makeButtons()
{
    for (auto b : std::as_const(_buttons))
        _groupBox->layout()->removeWidget(b);
    qDeleteAll(_buttons);
    _buttons.clear();
    
    auto presets = loadPresets();
    for (int i = 0; i < presets->size(); i++) {
        const auto &preset = presets->at(i);

        QList<QAction*> actions;

        auto b = new QPushButton(preset["name"].toString());
        b->setContextMenuPolicy(Qt::ActionsContextMenu);
        b->setFixedHeight(BUTTON_HEIGHT);
        connect(b, &QPushButton::clicked, this, &Self::applyPreset);
        _groupBox->layout()->addWidget(b);
        _buttons << b;

        auto actInfo = new QAction(QIcon(":/toolbar/info"), tr("Preset info..."), b);
        connect(actInfo, &QAction::triggered, this, &Self::showPresetInfo);
        actions << actInfo;

        auto actRename = new QAction(tr("Rename preset..."), b);
        connect(actRename, &QAction::triggered, this, &Self::renamePreset);
        actions << actRename;

        auto actUpdate = new QAction(tr("Save current values to preset..."), b);
        connect(actUpdate, &QAction::triggered, this, &Self::updatePreset);
        actions << actUpdate;

        auto actSep = new QAction(b);
        actSep->setSeparator(true);
        actions << actSep;

        auto actDel = new QAction(QIcon(":/toolbar/trash"), tr("Remove preset..."), b);
        connect(actDel, &QAction::triggered, this, &Self::removePreset);
        actions << actDel;

        for (auto a : qAsConst(actions)) a->setData(i);
        b->addActions(actions);
    }
    // Leading space is an addition text-to-icon separator
    auto b = new QPushButton(tr(" Add new preset..."));
    b->setIcon(QIcon(":/toolbar/plus"));
    b->setFixedHeight(BUTTON_HEIGHT);
    connect(b, &QPushButton::clicked, this, &Self::saveNewPreset);
    _groupBox->layout()->addWidget(b);
    _buttons << b;
    adjustHeight();
}

void CamPropPresets::adjustHeight()
{
    _groupBox->setFixedHeight(
        CamPropControls::groupTitleHeight()
        + CamPropControls::layoutTopMargin()
        + _buttons.count() * BUTTON_HEIGHT
        + (_buttons.count() - 1) * CamPropControls::layoutSpacing()
        + CamPropControls::layoutBottomMargin()
    );
}

#define PRESET_BY_INDEX(getIndex) \
    int index = (getIndex); \
    auto presets = loadPresets(); \
    if (index < 0 || index >= presets->size()) { \
        qWarning() << "Preset index out of range:" << index << "| Presets count:" << presets->size(); \
        return; \
    } \
    auto& preset = (*presets)[index];
    
#define PPESET_BY_ACTION \
    PRESET_BY_INDEX(qobject_cast<QAction*>(sender())->data().toInt())

void CamPropPresets::applyPreset()
{
    PRESET_BY_INDEX(_buttons.indexOf(sender()));
    
    QStringList failed;

    for (auto it = preset.cbegin(); it != preset.cend(); it++) {
        QString propName = it.key();
        if (propName == PRESET_NAME_KEY)
            continue;
        auto propId = CamProp::parseId(it.key());
        if (!propId) {
            qWarning() << _logId << "Unknown " << propName << "in preset" << preset.value(PRESET_NAME_KEY);
            continue;
        }
        bool ok;
        double value = it.value().toDouble(&ok);
        if (!ok) {
            qWarning() << _logId << "Invalid value of" << propName 
                << "in preset" << preset.value(PRESET_NAME_KEY) << ":" << it.value();
            continue;
        }
        if (!applyProp(*propId, value)) {
            failed << propName;
            qDebug() << _logId << "Preset not fully applied:" << propName
                << "| preset =" << value << "| camera =" << getPropValue(*propId);
        }
    }

    if (!failed.empty())
        Ori::Dlg::Defer::warning(tr("Preset are not fully applied. Failed to set some properties: %1").arg(failed.join(", ")));
    else
        Ori::Gui::PopupMessage::affirm(tr("Properties applied"));
}

void CamPropPresets::showPresetInfo()
{
    PPESET_BY_ACTION;
    Ori::Dlg::info(formatPreset(preset));
}

void CamPropPresets::renamePreset()
{
    PPESET_BY_ACTION;
    Ori::Dlg::InputTextOptions opts;
    opts.value = preset[PRESET_NAME_KEY].toString();
    opts.label = formatPreset(preset) + tr("<p>A new name for preset:</p>");
    opts.onHelp = []{ HelpSystem::topic("exp_presets"); };
    if (Ori::Dlg::inputText(opts)) {
        _buttons.at(index)->setText(opts.value);
        preset[PRESET_NAME_KEY] = opts.value;
        savePresets();
    }
}

void CamPropPresets::updatePreset()
{
    PPESET_BY_ACTION;
    auto newPreset = makePreset();
    newPreset[PRESET_NAME_KEY] = preset[PRESET_NAME_KEY];
    QString confirm = formatPreset(newPreset) +
        tr("<p>Update <b>%1</b> with these values?</p>").arg(preset[PRESET_NAME_KEY].toString());
    if (Ori::Dlg::yes(confirm)) {
        (*presets)[index] = newPreset;
        savePresets();
    }
}

void CamPropPresets::removePreset()
{
    PPESET_BY_ACTION;
    if (Ori::Dlg::yes(tr("The preset will be removed:<br><br><b>%1</b>").arg(preset[PRESET_NAME_KEY].toString()))) {
        presets->removeAt(index);
        savePresets();
        auto button = _buttons.at(index);
        _buttons.removeAt(index);
        button->deleteLater();
        adjustHeight();
        adjustParentHeight();
    }
}

void CamPropPresets::saveNewPreset()
{
    auto preset = makePreset();
    if (preset.isEmpty())
        return;
    Ori::Dlg::InputTextOptions opts;
    opts.label = formatPreset(preset) + tr("<p>Enter preset name:</p>");
    opts.onHelp = []{ HelpSystem::topic("exp_presets"); };
    if (Ori::Dlg::inputText(opts)) {
        preset[PRESET_NAME_KEY] = opts.value;
        auto presets = loadPresets();
        presets->append(preset);
        savePresets();
        makeButtons();
        adjustParentHeight();
    }
}

AnyRecord CamPropPresets::makePreset()
{
    AnyRecord preset;
    for (auto id : CamProp::wellKnownIds()) {
        auto value = getPropValue(id);
        if (!qIsNaN(value))
            preset[CamProp::getName(id)] = value;
    }
    return preset;
}

QString CamPropPresets::formatPreset(const AnyRecord &preset)
{
    QList<QPair<QString, double>> namedValues;
    int maxNameLen = 0;
    QSet<QString> addedProps;
    for (auto propId : CamProp::wellKnownIds()) {
        auto propName = CamProp::getName(propId);
        if (!preset.contains(propName))
            continue;
        auto displayName = CamProp::getDisplayName(propId);
        if (displayName.isEmpty())
            displayName = propName;
        displayName += ':';
        maxNameLen = qMax(maxNameLen, displayName.length());
        namedValues << qMakePair(displayName, preset.value(propName).toDouble());
        addedProps << propName;
    }
    for (auto it = preset.cbegin(); it != preset.cend(); it++) {
        auto propName = it.key();
        if (propName == PRESET_NAME_KEY)
            continue;
        if (addedProps.contains(propName))
            continue;
        maxNameLen = qMax(maxNameLen, propName.length());
        namedValues << qMakePair(propName, preset.value(propName).toDouble());
    }
    QStringList s;
    s << QStringLiteral("<code><pre>");
    for (const auto& namedValue : std::as_const(namedValues)) {
        // Trailing spaces are right margin for better look in the info window
        s << QStringLiteral("%1<b>%2</b><br>")
            .arg(namedValue.first.leftJustified(maxNameLen+2, ' '))
            .arg(namedValue.second);
    }
    s << QStringLiteral("</pre></code>");
    return s.join(QString());
}
