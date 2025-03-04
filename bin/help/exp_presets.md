# Exposure Presets

It's possible to save a set of exposure settings for a given camera as a preset. Presets are listed as buttons at the top of the control panel. Autoexposure level is saved into a preset too, even though it's not a camera setting and doesn't affect the exposure directly.

![Exposure presets](./img/cam_control_exp_presets.png)

Click a button to apply the preset. If currently selected settings, even being changed manually, match a preset, the button is highlighted to show that the preset is active. There are some issues with how IDS cameras apply settings; see [Camera control tweaks](./app_settings_opts.md#tweaks) for details.

Use the context menu to manage presets.

When [crosshair overlays](./overlays.md) are exported into a JSON file, the presets are saved into the file too. The same as crosshairs, preset edits are not reflected to the file automatically until it is reexported via the `[Overlays ► Save To File…]` menu command.

## Commands

### Add new preset

Click the button to save current exposure settings as a new preset. The application asks for a name for the preset. The name can be anything human-readable, but it's recommended to use a short, descriptive name.

### Preset info

The context menu command shows text information about settings' values saved in the preset.

### Rename preset

The context menu command renames the preset without changing its settings.

### Save current values to preset

The context menu command updates the existing preset with settings values that are currently applied to the camera.

### Remove preset

The context menu command removes the preset from the list.

## See also

- [Camera control](./cam_control.md)
- [Camera control tweaks](./app_settings_opts.md)

