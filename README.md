# OBS Plugin: PulseOBS - Webcam Based Heart Rate Monitoring for Live Streamers
PulseOBS is a plugin that measures your heart rate directly using your webcam and facial detection. It supports both macOS and Windows.

## Introduction

PulseOBS is designed to help live streamers monitor their heart rate without the need for wearable devices. Currently, it supports heart rate measurement for a single person visible in the webcam feed.

> **Note:** This plugin should not be used for medical purposes and does not provide any medical advice.

> **Warning:** This plugin consumes significant CPU resources.

## Download

### Windows
(TODO: Add download link and instructions)

### MacOs
Download the installation package from (TODO: Add link).

1. Double-click PACKAGE_NAME.pkg to open the installer.

2. Follow the on-screen instructions to complete the installation.

3. The plugin will be installed in:
    ```
    /Users/<user_name>/Library/Application\ Support/obs-studio/plugins
    ```

### OBS Version Support and Compatibility
PulseOBS supports the latest OBS version 31.0.1.

## Quick Start

For a step-by-step guide, visit (TODO: Add quick start guide link).

### How to Use

1. Add a `Video Capture Device` as a source in OBS Studio.

2. Select `Video Capture Device` and click `Filters`.

3. Under `Effect Filters`, select `Heart Rate Monitor`.

4. Click `Heart Rate Monitor` to configure settings.

5. The heart rate can be displayed as a text source or a graph source.

## Support and Feedback

For assistance, please contact: (TODO: Add support email).

To report bugs or suggest features, open an issue at (TODO: Add issue tracker link).

## Technical Details

### Heart Rate Calculation
PulseOBS provides multiple algorithms to calculate heart rate:
- Green
- PCA
- Chrom

### Filtering
Pre and Post Filtering methods are used to improve accuracy:

Pre-Filtering:
- Bandpass

Post-Filtering:
- Bandpass
- Detrending
- Zero Mean

### Face Detection and Tracking
PulseOBS uses face detection and tracking to optimize processing speed and improve accuracy. Available face detection methods:

- OpenCV Haar Cascade

- dlib HoG (with and without face tracking)
    - Face detection is performed every 60 frames.
    - Face tracking is used between detections to enhance performance.

## Build Instructions

PulseOBS is built and tested on macOS (Intel) and Windows. Contributions for additional OS support are welcome.

This plugin utilizes the following libraries:
- OpenCV
- dlib
- Eigen3

### Supported Build Environments

| Platform  | Tool   |
|-----------|--------|
| Windows   | Visal Studio 17 2022 |
| macOS     | XCode 16.0 |
| Windows, macOS  | CMake 3.30.5 |
<!-- | Ubuntu 24.04 | CMake 3.28.3 |
| Ubuntu 24.04 | `ninja-build` |
| Ubuntu 24.04 | `pkg-config`
| Ubuntu 24.04 | `build-essential` | -->

### Windows

Run the following command in the repository directory:
```
.github/scripts/Build-Windows.ps1
```
This will install all required third-party libraries and compile the plugin into (TODO: Add release directory).

### MacOS

Run the following command in the repository directory:
```
.github/scripts/build-macos
```
TThis will install all required third-party libraries and compile the plugin into: 
```
release/RelWithDebInfo
``` 
which contains `pluse-obs.plugin` and `pulse-obs.pkg`.
