# Building and testing

```bash
git clone https://github.com/orion-project/beam-inspector.git
cd beam-inspector
git submodule update --init --recursive
```

By default, the application uses [Qt Creator](https://www.qt.io/download-dev) IDE and Qt 6.8+ for building the project. Just open the project's `CMakeLists.txt` in Qt Creator and configure it to build with an installed Qt kit.

You can also build the application using CMake directly (TODO: explain) or using [Visual Studio Code](https://code.visualstudio.com/) extension [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) (TODO: explain).

Currently, only Windows is supported. (TODO: IDS provides the Linux version of their SDK)

## IDS Peak SDK Installation Note

For physical cameras, currently, only IDS cameras are supported via the [IDS Peak SDK](https://en.ids-imaging.com/download-peak.html). The latest tried version is 2.12.0.0. The SDK is not included in this repository, but needs to be installed separately.

When installing the IDS Peak SDK, it should be installed into a folder without spaces (e.g., into a "Programs" directory rather than "Program Files"). Otherwise, you may encounter errors like:

```
Files/IDS/ids_peak/comfort_sdk/api/include: No such file or directory
```

This is likely due to how the SDK's CMake files handle paths with spaces.

TODO: Support default installation directory (`Program Files`) or add a clean explanation why it is necessary to install the SDK into a directory without spaces.

TODO: Support SDK version 2.13.0.0 or higher. E.g. when version 2.15 is installed, the IDE tries to find *lib*ids_peak_comfort_c.dll (with the *lib* prefix) which obviously does not exist.

## Virtual cameras

Virtual cameras used for development of common (not hardware related) features in the absence of physical device. They are available when the app is running with the `--dev` command line option.

TODO: add explanation