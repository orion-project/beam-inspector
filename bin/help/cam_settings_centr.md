# Camera Settings - Centroid

```
► Camera ► Settings ► Centroid
```

![Screenshot](./img/cam_settings_centr.png)

## Options

Options control how to calculate beam centroid parameters (position, diameters). These options only applied when [background subtraction](./cam_settings_bgnd.md) is enabled. Otherwise the simple [calculation](./iso.md) over unmodified image data (respecting [ROI](./cam_settings_roi.md)) is performed.

### Mask Diameter

According to ISO 11146-1 all [integrations](./iso.md) should be performed in an area centered at the beam and several times larger than the beam width. Since the beam width is initially unknown, the first integration is done over the whole image (respecting [ROI](./cam_settings_roi.md)), and obtained centroid parameters used for defining the new integration area. From which new centroid parameters are calculated, then the next integration area and so on.

The mask diameter defines now many times the integration area size is larger than the beam width. Recommended value is 3.

### Max Iterations

The parameter defines how many times integration area should be calculated from the previous centroid parameters. If it is 0 then there are no iterations. So centroid parameters are only obtained from the whole image (respecting [ROI](./cam_settings_roi.md)) and no new integration area is calculated.

### Precision

If the centroid parameters calculated on an iteration differ from parameters obtained on the previous iteration not more that given percent then iterations stop.

Practically, on good images it's often enough to calculate centroid parameters only once (Max Iterations = 0). 

## See also

- [Background subtraction](./cam_settings_bgnd.md)
- [ISO 11146 Equations](./iso.md)
- [ROI](./cam_settings_roi.md)

&nbsp;
