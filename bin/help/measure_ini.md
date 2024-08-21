```ini
; Configuration section
; This section is read when the file is loaded as measurement preset
[Measurement]
timestamp=2024-08-21T22:38:37
imageDir=G:/Projects/beam-inspector/tmp/out/measure1.images
fileName=G:/Projects/beam-inspector/tmp/out/measure1.csv
intervalSecs=1
average=true
duration=1m
imgInterval=5s

[Stats]
elapsedTime=1m
resultsSaved=59
imagesSaved=12
framesDropped=0
framesIncomplete=0
framesUnderrun=0

[Camera]
name=U3-368xXLE-M (*5988)
resolution=2592 × 1944 × 8bit
sensorScale.on=true
sensorScale.factor=2.2
sensorScale.unit=um
exposure=15003.793333333333
frameRate=20

[CameraSettings]
plot.normalize=true
plot.rescale=true
plot.fullRange=true
plot.customScale.on=false
bgnd.on=true
bgnd.iters=0
bgnd.precision=0.05
bgnd.corner=0.035
bgnd.noise=3
bgnd.mask=3
roi.on=false
```