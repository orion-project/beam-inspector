#ifndef PROFILES_VIEW_H
#define PROFILES_VIEW_H

#include "cameras/CameraTypes.h"
#include "widgets/PlotHelpers.h"

#include <QQueue>
#include <QWidget>

class PlotIntf;

class QCPAxisRect;
class QCPGraph;
class QCPTextElement;
class QCustomPlot;

class ProfilesView : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilesView(PlotIntf *plotIntf);

    void restoreState(QSettings *s);
    void setThemeColors(PlotHelpers::Theme theme, bool replot);
    void setConfig(const PixelScale& scale, const Averaging& mavg);
    void showResult();
    void cleanResult();
    void activate(bool on);

    struct Point { double key, value ;};
    using Points = QVector<Point>;
    struct Profile { Points x; Points y; };

private:
    PlotIntf *_plotIntf;
    QCustomPlot *_plotX, *_plotY;
    //QCPGraph *_rawX, *_rawY;
    QCPGraph *_profileX, *_profileY, *_fitX, *_fitY;
    Points _lastX, _lastY;
    QQueue<Profile> _profiles;
    QCPTextElement *_textMiX, *_textMiY;
    PixelScale _scale;
    QAction *_actnShowFit, *_actnCopyFitX, *_actnCopyFitY, *_actnSetMI, *_actnCenterFit, *_actnShowFullY;
    double _profileRange = 2;
    int _pointCount = 100;
    double _rangeX = 0;
    double _rangeY = 0;
    double _MI = 1.0; // M-square value for hyper-gaussian fit
    bool _showFit = true;
    bool _centerFit = false;
    bool _showFullY = false;
    int _mavgFrames = 0;
    bool _active = false;

    void updateVisibility();
    void storeState();
    void toggleShowFit();
    void toggleCenterFit();
    void toggleShowFullY();
    void setMI();
    void showMI();
    void autoScale();
    int totalPoints() const { return 2*_pointCount - 1; }
};

#endif // PROFILES_VIEW_H
