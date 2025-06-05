#ifndef HEATMAP_H
#define HEATMAP_H

#include "qcustomplot/src/axis/range.h"
#include "qcustomplot/src/colorgradient.h"
#include "qcustomplot/src/plottable.h"

class HeatmapData
{
public:
  HeatmapData();
  ~HeatmapData();
  
  int keySize() const { return mKeySize; }
  int valueSize() const { return mValueSize; }
  QCPRange keyRange() const { return mKeyRange; }
  QCPRange valueRange() const { return mValueRange; }
  double data(double key, double value);
  double cell(int keyIndex, int valueIndex);
  
  void setSize(int keySize, int valueSize);
  void setRange(const QCPRange &keyRange, const QCPRange &valueRange);
  void setData(double key, double value, double z);
  void setCell(int keyIndex, int valueIndex, double z);
  
  void clear();
  void fill(double z);
  bool isEmpty() const { return !mData || mKeySize == 0 || mValueSize == 0; }
  void coordToCell(double key, double value, int *keyIndex, int *valueIndex) const;

protected:
  int mKeySize = 0, mValueSize = 0;
  QCPRange mKeyRange, mValueRange;

  double *mData = nullptr;
  bool mDataModified = false;
  
  friend class Heatmap;
};

/**
 * This is a simplified clone of QCPColorMap.
 * The original graph draws a colored cell around a value.
 * So the boundary values are drawn as half-cells:
 * 
 * value: 0      1       2      3     (value range = 3)
 *        |---|------|------|---|
 * cell     0     1     2     3       (cell count = 4)
 * 
 * Or they optionally can be drawns as full cells when setTightBoundary(false)
 * but this does't care any additional information.
 * 
 * The Heatmap draws cells between values.
 * This is useful for showing how many values fall into a region.
 * 
 * value: 0      1      2      3      (value range = 3)
 *        |------|------|------|
 * cell:      0      1      2         (cell count = 3)
 * 
 * The value itself falls into the right neighbour cell.
 * The exclusion is the very-last value which falls into the left cell.
*/
class Heatmap : public QCPAbstractPlottable
{
public:
  explicit Heatmap(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~Heatmap() override;
  
  HeatmapData *data() const { return mMapData; }
  QCPRange dataRange() const { return mDataRange; }
  bool interpolate() const { return mInterpolate; }
  QCPColorGradient gradient() const { return mGradient; }
  
  void setData(HeatmapData *data);
  void setDataRange(const QCPRange &dataRange);
  void setGradient(const QCPColorGradient &gradient);
  void setInterpolate(bool enabled);
  
  double selectTest(const QPointF &pos, bool onlySelectable, QVariant *details=nullptr) const override { return -1; }
  QCPRange getKeyRange(bool &foundRange, QCP::SignDomain inSignDomain=QCP::sdBoth) const override;
  QCPRange getValueRange(bool &foundRange, QCP::SignDomain inSignDomain=QCP::sdBoth, const QCPRange &inKeyRange=QCPRange()) const override;
  
protected:
  QCPRange mDataRange;
  HeatmapData *mMapData = nullptr;
  QCPColorGradient mGradient = QCPColorGradient::gpCold;
  bool mInterpolate = true;
  
  QImage mMapImage, mUndersampledMapImage;
  bool mMapImageInvalidated = true;
  
  void updateMapImage();
  
  void draw(QCPPainter *painter) override;
  void drawLegendIcon(QCPPainter *painter, const QRectF &rect) const override {}
};

#endif // HEATMAP_H
