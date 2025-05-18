# Version Info Extractor

A Win32 command-line application that extracts version information from executable (.exe) or dynamic-link library (.dll) files.

## Description

Version Info Extractor is a utility that helps you retrieve the version information embedded in Windows binary files. It works by parsing the PE (Portable Executable) file format and extracting the version resource information. This can be useful for:

- Identifying software versions
- Verifying installed components
- Troubleshooting version compatibility issues
- Automating software inventory

## Features

- Analyzes both EXE and DLL files
- Extracts and displays version numbers in standard format (e.g., 2.15.0.0)
- Provides proper error handling for invalid files
- Uses the Windows API to access version information resources
- Lightweight and portable

## Requirements

- Windows operating system
- C++ compiler (g++)

## Building the Application

### Using g++

```
g++ -std=c++11 -Wall -static -o ver.exe ver.cpp -lversion
```

The `-lversion` flag links against the version.lib library, which is required for the version information functions.

## Usage

```
ver.exe <path_to_file>
```

Where `<path_to_file>` is the path to the EXE or DLL you want to analyze.

### Examples

Analyze a system DLL:

```
ver.exe C:\Windows\System32\kernel32.dll
```

Sample output:
```
10.0.19041.2788
```

Analyze an application:

```
ver.exe C:\Programs\IDS\program\ids_peak_cockpit.exe
```

Sample output:
```
115.0.3.0
```

## How It Works

The application:

1. Loads the target file's version information resource
2. Parses the version information structure
3. Extracts the file version numbers
4. Displays the version in the format "Major.Minor.Build.Revision"

## Technical Details

The application uses the following Windows API components:

- `GetFileVersionInfoSize` to determine the buffer size needed
- `GetFileVersionInfo` to retrieve the version information
- `VerQueryValue` to extract specific version information
- `VS_FIXEDFILEINFO` structure to access the version numbers

## License

This project is available under the MIT License.
