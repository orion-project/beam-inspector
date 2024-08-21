# Processing Images

```
► File ► Open Image [Ctrl+O]
```

Besides continuous acquisition of frames from cameras the app can also analyze previously captured static images. Open a file via the `[File ► Open Image]` menu command or switch the [camera selector](./cam_selector.md) to the Image mode. A name and resolution of the current image file is displayed in the [status bar](./status_bar.md). Features and [calculation results](./results_table.md) are the same as for cameras, including [background subtraction](./cam_settings_bgnd.md) and [raw view](./raw_view.md), [ROI](./cam_settings_roi.md) is also respected.

Gray-scale 8-bit and 16-bit PNG, PGM, and JPG formats are supported. If you see the message "Image format is not supported" this likely means the picture is colored. Currently there are no built-in algorithms for computing beam intensity from R,G,B channels.

There are several sample image files to play with in the `examples` subdirectory of the application installation folder.

&nbsp;
