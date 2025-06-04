#include "Heatmap.h"

#include "core/OriFloatingPoint.h"

#include "qcustomplot/src/painter.h"

//------------------------------------------------------------------------------
//                                HeatmapData
//------------------------------------------------------------------------------

HeatmapData::HeatmapData(int keySize, int valueSize) :
  mKeySize(0),
  mValueSize(0),
  mKeyRange(QCPRange(0, 1)),
  mValueRange(QCPRange(0, 1)),
  mData(nullptr),
  mDataModified(true)
{
  setSize(keySize, valueSize);
  fill(0);
}

HeatmapData::~HeatmapData()
{
  delete[] mData;
}

double HeatmapData::data(double key, double value)
{
  int keyCell = (key-mKeyRange.lower)/(mKeyRange.upper-mKeyRange.lower)*mKeySize;
  int valueCell = (value-mValueRange.lower)/(mValueRange.upper-mValueRange.lower)*mValueSize;
  if (keyCell >= 0 && keyCell < mKeySize && valueCell >= 0 && valueCell < mValueSize)
    return mData[valueCell*mKeySize + keyCell];
  else
    return 0;
}

double HeatmapData::cell(int keyIndex, int valueIndex)
{
  if (keyIndex >= 0 && keyIndex < mKeySize && valueIndex >= 0 && valueIndex < mValueSize)
    return mData[valueIndex*mKeySize + keyIndex];
  else
    return 0;
}

void HeatmapData::setSize(int keySize, int valueSize)
{
  if (keySize != mKeySize || valueSize != mValueSize)
  {
    mKeySize = keySize;
    mValueSize = valueSize;
    delete[] mData;
    if (mKeySize > 0 && mValueSize > 0)
    {
#ifdef __EXCEPTIONS
      try { // 2D arrays get memory intensive fast. So if the allocation fails, at least output debug message
#endif
      mData = new double[size_t(mKeySize*mValueSize)];
#ifdef __EXCEPTIONS
      } catch (...) { mData = nullptr; }
#endif
      if (mData)
        fill(0);
      else
        qDebug() << Q_FUNC_INFO << "out of memory for data dimensions "<< mKeySize << "*" << mValueSize;
    } else
      mData = nullptr;
    
    mDataModified = true;
  }
}

void HeatmapData::setRange(const QCPRange &keyRange, const QCPRange &valueRange)
{
  mKeyRange = keyRange;
  mValueRange = valueRange;
}

void HeatmapData::setData(double key, double value, double z)
{
  int keyCell = (key-mKeyRange.lower)/(mKeyRange.upper-mKeyRange.lower)*mKeySize;
  int valueCell = (value-mValueRange.lower)/(mValueRange.upper-mValueRange.lower)*mValueSize;
  if (keyCell >= 0 && keyCell < mKeySize && valueCell >= 0 && valueCell < mValueSize)
  {
    mData[valueCell*mKeySize + keyCell] = z;
    mDataModified = true;
  }
}

void HeatmapData::setCell(int keyIndex, int valueIndex, double z)
{
  if (keyIndex >= 0 && keyIndex < mKeySize && valueIndex >= 0 && valueIndex < mValueSize)
  {
    mData[valueIndex*mKeySize + keyIndex] = z;
    mDataModified = true;
  } else
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << keyIndex << valueIndex;
}

void HeatmapData::fill(double z)
{
  if (isEmpty()) return;
  const int dataCount = mValueSize*mKeySize;
  memset(mData, z, dataCount*sizeof(*mData));
  mDataModified = true;
}

void HeatmapData::coordToCell(double key, double value, int *keyIndex, int *valueIndex) const
{
    // Should not be rounding here
    // For example: range=(-15..15), cells=3 => cellSize=10
    //
    // -15        -5         5         15
    //   |---------|---------|---------|
    // Index:  0        1         2
    //
    // If value=0 then (0-(-15)/10) = 15/10 = 1.5 => cellIndex=1
    // while with rounding it would go the cellIndex=2, which is wrong
    
  if (keyIndex) {
    *keyIndex = (key-mKeyRange.lower)/(mKeyRange.upper-mKeyRange.lower)*mKeySize;
    if (*keyIndex >= mKeySize && SAME_DOUBLE(key, mKeyRange.upper))
      *keyIndex = mKeySize-1;
  }
  if (valueIndex) {
    *valueIndex = (value-mValueRange.lower)/(mValueRange.upper-mValueRange.lower)*mValueSize;
    if (*valueIndex >= mValueSize && SAME_DOUBLE(value, mValueRange.upper))
      *valueIndex = mValueSize-1;
  }
}

//------------------------------------------------------------------------------
//                                  Heatmap
//------------------------------------------------------------------------------

Heatmap::Heatmap(QCPAxis *keyAxis, QCPAxis *valueAxis) : QCPAbstractPlottable(keyAxis, valueAxis)
{
}

Heatmap::~Heatmap()
{
  if (mMapData)
    delete mMapData;
}

void Heatmap::setData(HeatmapData *data)
{
  if (mMapData == data)
    return;
  if (mMapData)
    delete mMapData;
  mMapData = data;
  mMapImageInvalidated = true;
}

void Heatmap::setDataRange(const QCPRange &dataRange)
{
  if (!QCPRange::validRange(dataRange)) return;
  if (mDataRange.lower != dataRange.lower || mDataRange.upper != dataRange.upper)
  {
    mDataRange = dataRange.sanitizedForLinScale();
    mMapImageInvalidated = true;
  }
}

void Heatmap::setGradient(const QCPColorGradient &gradient)
{
  if (mGradient != gradient)
  {
    mGradient = gradient;
    mMapImageInvalidated = true;
  }
}

void Heatmap::setInterpolate(bool enabled)
{
  mInterpolate = enabled;
  mMapImageInvalidated = true; // because oversampling factors might need to change
}

QCPRange Heatmap::getKeyRange(bool &foundRange, QCP::SignDomain inSignDomain) const
{
  foundRange = true;
  QCPRange result = mMapData->keyRange();
  result.normalize();
  if (inSignDomain == QCP::sdPositive)
  {
    if (result.lower <= 0 && result.upper > 0)
      result.lower = result.upper*1e-3;
    else if (result.lower <= 0 && result.upper <= 0)
      foundRange = false;
  } else if (inSignDomain == QCP::sdNegative)
  {
    if (result.upper >= 0 && result.lower < 0)
      result.upper = result.lower*1e-3;
    else if (result.upper >= 0 && result.lower >= 0)
      foundRange = false;
  }
  return result;
}

QCPRange Heatmap::getValueRange(bool &foundRange, QCP::SignDomain inSignDomain, const QCPRange &inKeyRange) const
{
  if (inKeyRange != QCPRange())
  {
    if (mMapData->keyRange().upper < inKeyRange.lower || mMapData->keyRange().lower > inKeyRange.upper)
    {
      foundRange = false;
      return {};
    }
  }
  
  foundRange = true;
  QCPRange result = mMapData->valueRange();
  result.normalize();
  if (inSignDomain == QCP::sdPositive)
  {
    if (result.lower <= 0 && result.upper > 0)
      result.lower = result.upper*1e-3;
    else if (result.lower <= 0 && result.upper <= 0)
      foundRange = false;
  } else if (inSignDomain == QCP::sdNegative)
  {
    if (result.upper >= 0 && result.lower < 0)
      result.upper = result.lower*1e-3;
    else if (result.upper >= 0 && result.lower >= 0)
      foundRange = false;
  }
  return result;
}

void Heatmap::updateMapImage()
{
  QCPAxis *keyAxis = mKeyAxis.data();
  if (!keyAxis) return;
  if (mMapData->isEmpty()) return;
  
  const QImage::Format format = QImage::Format_ARGB32_Premultiplied;
  const int keySize = mMapData->keySize();
  const int valueSize = mMapData->valueSize();
  int keyOversamplingFactor = mInterpolate ? 1 : int(1.0+100.0/double(keySize)); // make mMapImage have at least size 100, factor becomes 1 if size > 200 or interpolation is on
  int valueOversamplingFactor = mInterpolate ? 1 : int(1.0+100.0/double(valueSize)); // make mMapImage have at least size 100, factor becomes 1 if size > 200 or interpolation is on
  
  // resize mMapImage to correct dimensions including possible oversampling factors, according to key/value axes orientation:
  if (keyAxis->orientation() == Qt::Horizontal && (mMapImage.width() != keySize*keyOversamplingFactor || mMapImage.height() != valueSize*valueOversamplingFactor))
    mMapImage = QImage(QSize(keySize*keyOversamplingFactor, valueSize*valueOversamplingFactor), format);
  else if (keyAxis->orientation() == Qt::Vertical && (mMapImage.width() != valueSize*valueOversamplingFactor || mMapImage.height() != keySize*keyOversamplingFactor))
    mMapImage = QImage(QSize(valueSize*valueOversamplingFactor, keySize*keyOversamplingFactor), format);
  
  if (mMapImage.isNull())
  {
    qDebug() << Q_FUNC_INFO << "Couldn't create map image (possibly too large for memory)";
    mMapImage = QImage(QSize(10, 10), format);
    mMapImage.fill(Qt::black);
  } else
  {
    QImage *localMapImage = &mMapImage; // this is the image on which the colorization operates. Either the final mMapImage, or if we need oversampling, mUndersampledMapImage
    if (keyOversamplingFactor > 1 || valueOversamplingFactor > 1)
    {
      // resize undersampled map image to actual key/value cell sizes:
      if (keyAxis->orientation() == Qt::Horizontal && (mUndersampledMapImage.width() != keySize || mUndersampledMapImage.height() != valueSize))
        mUndersampledMapImage = QImage(QSize(keySize, valueSize), format);
      else if (keyAxis->orientation() == Qt::Vertical && (mUndersampledMapImage.width() != valueSize || mUndersampledMapImage.height() != keySize))
        mUndersampledMapImage = QImage(QSize(valueSize, keySize), format);
      localMapImage = &mUndersampledMapImage; // make the colorization run on the undersampled image
    } else if (!mUndersampledMapImage.isNull())
      mUndersampledMapImage = QImage(); // don't need oversampling mechanism anymore (map size has changed) but mUndersampledMapImage still has nonzero size, free it
    
    const double *rawData = mMapData->mData;
    if (keyAxis->orientation() == Qt::Horizontal)
    {
      const int lineCount = valueSize;
      const int rowCount = keySize;
      for (int line=0; line<lineCount; ++line)
      {
        QRgb* pixels = reinterpret_cast<QRgb*>(localMapImage->scanLine(lineCount-1-line)); // invert scanline index because QImage counts scanlines from top, but our vertical index counts from bottom (mathematical coordinate system)
        mGradient.colorize(rawData+line*rowCount, mDataRange, pixels, rowCount, 1, false);
      }
    } else // keyAxis->orientation() == Qt::Vertical
    {
      const int lineCount = keySize;
      const int rowCount = valueSize;
      for (int line=0; line<lineCount; ++line)
      {
        QRgb* pixels = reinterpret_cast<QRgb*>(localMapImage->scanLine(lineCount-1-line)); // invert scanline index because QImage counts scanlines from top, but our vertical index counts from bottom (mathematical coordinate system)
        mGradient.colorize(rawData+line, mDataRange, pixels, rowCount, lineCount, false);
      }
    }
    
    if (keyOversamplingFactor > 1 || valueOversamplingFactor > 1)
    {
      if (keyAxis->orientation() == Qt::Horizontal)
        mMapImage = mUndersampledMapImage.scaled(keySize*keyOversamplingFactor, valueSize*valueOversamplingFactor, Qt::IgnoreAspectRatio, Qt::FastTransformation);
      else
        mMapImage = mUndersampledMapImage.scaled(valueSize*valueOversamplingFactor, keySize*keyOversamplingFactor, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
  }
  mMapData->mDataModified = false;
  mMapImageInvalidated = false;
}

void Heatmap::draw(QCPPainter *painter)
{
  if (mMapData->isEmpty()) return;
  if (!mKeyAxis || !mValueAxis) return;
  applyDefaultAntialiasingHint(painter);
  
  if (mMapData->mDataModified || mMapImageInvalidated)
    updateMapImage();
  
  // use buffer if painting vectorized (PDF):
  const bool useBuffer = painter->modes().testFlag(QCPPainter::pmVectorized);
  QCPPainter *localPainter = painter; // will be redirected to paint on mapBuffer if painting vectorized
  QRectF mapBufferTarget; // the rect in absolute widget coordinates where the visible map portion/buffer will end up in
  QPixmap mapBuffer;
  if (useBuffer)
  {
    const double mapBufferPixelRatio = 3; // factor by which DPI is increased in embedded bitmaps
    mapBufferTarget = painter->clipRegion().boundingRect();
    mapBuffer = QPixmap((mapBufferTarget.size()*mapBufferPixelRatio).toSize());
    mapBuffer.fill(Qt::transparent);
    localPainter = new QCPPainter(&mapBuffer);
    localPainter->scale(mapBufferPixelRatio, mapBufferPixelRatio);
    localPainter->translate(-mapBufferTarget.topLeft());
  }
  
  QRectF imageRect = QRectF(coordsToPixels(mMapData->keyRange().lower, mMapData->valueRange().lower),
                            coordsToPixels(mMapData->keyRange().upper, mMapData->valueRange().upper)).normalized();
  const bool mirrorX = (keyAxis()->orientation() == Qt::Horizontal ? keyAxis() : valueAxis())->rangeReversed();
  const bool mirrorY = (valueAxis()->orientation() == Qt::Vertical ? valueAxis() : keyAxis())->rangeReversed();
  const bool smoothBackup = localPainter->renderHints().testFlag(QPainter::SmoothPixmapTransform);
  localPainter->setRenderHint(QPainter::SmoothPixmapTransform, mInterpolate);
  localPainter->drawImage(imageRect, mMapImage.mirrored(mirrorX, mirrorY));
  localPainter->setRenderHint(QPainter::SmoothPixmapTransform, smoothBackup);
  
  if (useBuffer) // localPainter painted to mapBuffer, so now draw buffer with original painter
  {
    delete localPainter;
    painter->drawPixmap(mapBufferTarget.toRect(), mapBuffer);
  }
}
