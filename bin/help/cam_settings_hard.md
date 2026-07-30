# Camera Settings

Camera settings are stored on a per-camera basis, so when another [camera is selected](./cam_selector.md), then other settings will be loaded. The model name of the camera and its serial number used as a key for settings string. And there is a separate set of settings for [static images processing](./static_img.md), they do not differ for different images.

## Hardware

```
► Camera ► Settings ► Hardware
```

![Screenshot](./img/cam_settings_hard.png)

### Pixel format

[Pixel format](https://www.1stvision.com/cameras/IDS/IDS-manuals/en/basics-image-color-formats.html) defines a way how camera packs pixel data into resulting byte array. For IDS cameras, supported formats are `Mono8` (8 bit), `Mono10` and `Mono10G40` (10 bit), `Mono12` and `Mono12G24` (12 bit). [Bit depth](https://www.1stvision.com/cameras/IDS/IDS-manuals/en/basics-bit-depth.html) is the number of bits used to code the color of a single pixel. See camera specs and [official documentation](https://www.1stvision.com/cameras/IDS/IDS-manuals/en/basics-monochrome-pixel-formats.html) for details.

By default, this list shows all formats that the camera can provide and the app can decode and display. In a special debug mode activated via [application settings](./app_settings_debug.md), it is possible to select a pixel format that cannot be decoded and processed. For unsupported pixel formats, the app still receives data from the camera, and it's possible to save [raw frames](./export_raw.md) for some external processing. Unsupported items are marked with an exclamation icon in the list. When such an unsupported format is selected, the [status bar](./status_bar.md) displays frame resolution in <span style="color:red">red color</span> and shows zero bit depth.

After changing the option, re-select the camera in the [camera selector](./cam_selector.md) to apply.

### Resolution reduction

Resolution reduction techniques can be used to reduce the CPU load the bandwidth requirements on the USB.

**Binning** allows for reducing image size by averaging pixels over several rows or/and columns. See the [official documentation](https://www.1stvision.com/cameras/IDS/IDS-manuals/en/basics-binning.html) for details.

Some cameras support only combined binning mode, they can't average only rows or only columns separately; both X and Y values must be set to the same value. See the specs of your camera for details.

**Decimation** (or subsampling) allows for reducing image size by skipping several rows or/and columns of pixels. See the [official documentation](https://www.1stvision.com/cameras/IDS/IDS-manuals/en/basics-decimation.html) for details.

Cameras can not apply both binning and decimation at the same time, so when one option is set, the other gets locked.

After changing these options, re-select the camera in the [camera selector](./cam_selector.md) to apply.

## See also

- [Supported cameras](./hardware.md)
- [Camera selector](./cam_selector.md)

&nbsp;
