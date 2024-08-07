# Camera Settings - ROI

```
► Camera ► Settings ► ROI
```

![Screenshot](./img/cam_settings_roi.png)

## Options

ROI means *Region of Interest*.

### Use region

When enabled, only part of image used for [calculation](./cam_settings_bgnd.md) and [background subtraction](./cam_settings_bgnd.md). Borders of the region are drawn on the plot as dashed lines.

The flag can also be toggled via the `[Camera ► Use ROI]` menu command or respective button on the toolbar.

![ROI](./img/roi.png)

Some cameras allow for defining ROI on hardware or driver level. This camera feature is not currently used. The app always resets a camera ROI, acquires the full frame, and implements ROI as software bounds for processing data. No temporary cropped image is generated.

### Left, Top, Right, Bottom

ROI bounds are set in raw pixels. Even if the [Rescale pixels](./cam_settings_plot.md#rescale-pixels) option is activated, it is not used here. If it's important to define ROI bounds in physical units then use the [Live ROI](./roi_live.md) feature.

## Related commands

- `Camera ► Edit ROI`
- `Camera ► Use ROI`
- `View ► Zoom to Sensor`
- `View ► Zoom to ROI`

## See also

- [Background subtraction](./cam_settings_bgnd.md)
- [Centroid calculation](./cam_settings_centr.md)
- [Live ROI editing](./roi_live.md)

&nbsp;
