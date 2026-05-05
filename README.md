# Real Fake SDR Display in HDR

"Real Fake SDR Display in HDR" is an app allows you to visualize different ways to convert SDR images into HDR

It emulates logic of an SDR monitor, calculates colors a real monitor would produce, converts those colors into HDR and presents them to the screen

To visualize conversion of any SDR app:

* Switch to selected app
* Press Alt+Prt Scn to capture screenshot of relevant window
* Switch back to "Real Fake SDR Display in HDR"
* Press CTRL+V to paste your screenshot

Alternatively, you can drag & drop any SDR image file into this window, or paste any other image from your clipboard

# Settings

* Display Luminance Level - Analog of "Brightness" setting found on most monitors. Units are $cd/m^2$. Value of 80 $cd/m^2$ represents sRGB Reference Display property.
* Opto-Electronic Transfer Function (OETF) - Different functions to map digital values into luminance values
  * Power Function OETF - Simple $x^a$, where $a$ can be configured with "Power function characteristic" setting. Represents sRGB standard compliant transfer function. When "Power function characteristic" is set to 2.2, it represents sRGB Reference Display property.
  * sRGB inverse EOTF - Uses function that is inverse of stadard sRGB piecewise EOTF. Represent incorrect transform. Used by Windows for SDR to HDR conversion.
* Custom Color Gamut - Configures how to treat input sRGB colors.
  * When Disabled - you can configure color gamut with "Vividness" setting. When "Vividness" is set to 0, it represents sRGB Reference Display property. When you change the value, input colors are treated as if they are in the color gamut between sRGB and Rec. 2020, linearly interpolated with "Vividness" value as a coeficient (0.0 - sRGB, 1.0 - Rec. 2020).
  * When Enabled - you can manually set exact color gamut (except for the white point, which is locked to Illuminant D65).

# Building

## Prerequisites

* CMake 3.25 or Later
* Visual Studio 2022 with C++ development components installed
  * Other Generators or older versions of Visual Studio may work, but are not tested

## How to build

In the repository root:
1. Initialize git submodules 
```
git submodule update --init --recursive
```
2. Configure CMake project
```
cmake -S . -B build
```
3. Build generated project
```
cmake --build build -j --config Release
```

After build, artifacts will be in the `build/{Configuration}` folder
