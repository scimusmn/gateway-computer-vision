# gateway-computer-vision

## match-settings.xml

This file is the configuration file for the demo. The tags the file accepts are detailed below.

#### ROI identification parameters

These tags control the way the program looks for a region to match to a template.

| Tag                     | Function                                             |
|-------------------------|------------------------------------------------------|
| `canny-roi-low`         | Low threshold for the Canny edge finder.             |
| `canny-roi-high`        | High threshold for the Canny edge finder.            |
| `canny-roi-kernel`      | Kernel size for the Canny edge finder.               |
| `circularity-threshold` | Minimum level of circularity\*.                      |
| `minimum-roi-area`      | Minimum number of pixels to be acceptable as an ROI. |

\*The circularity of a contour is determined by

<img src="https://github.com/scimusmn/gateway-computer-vision/blob/master/img/circularity.png" alt="C = (4 PI A)/p^2">

where A is the area of the contour and p is the contour's perimeter.

#### Match parameters

These tags control the way the program matches an ROI onto a template.

| Tag                   | Function                                                      |
|-----------------------|---------------------------------------------------------------|
| `canny-match-low`     | Low threshold for the Canny edge finder.                      |
| `canny-match-high`    | High threshold for the Canny edge finder.                     |
| `canny-match-kernel`  | Kernel size for the Canny edge finder.                        |
| `vertical-segments`   | Number of sections to divide ROIs and templates vertically.   |
| `horizontal-segments` | Number of sections to divide ROIs and templates horizontally. |

#### Templates

The `<templates>` tag contains the template images and some ancillary data on what to
do with them. Each template is nested inside a `<_>...</_>` tag and contains the following tags:

| Tag      | Function                                                         |
|----------|------------------------------------------------------------------|
| `image`  | The image to use as a template.                                  |
| `name`   | The human-readable name of the template.                         |
| `signal` | The signal to send to the Arduino upon recognizing the template. |

#### General settings

These tags control more general behavior of the program

| Tag | Function |
|-----|----------|
| `camera`            | The number of the camera to load. On \*nix systems, this is `/dev/video\[number]`. |
| `serial-port`       | The serial port for sending data to the Arduino. |
| `window-name`       | The name of the window to display in.                                                                  |
| `counts-for-signal` | Number of frames to wait after matching with a template before sending a signal the the Arduino. |
