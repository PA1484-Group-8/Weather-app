# README.md

## Introduction

This project is a weather station system built using an **ESP32**, a
**touchscreen display**, and data fetched from the **SMHI weather
API**.
The purpose of the system is to give users an easy and interactive way
to view local weather information in real time. It solves the problem of
needing a simple, dedicated interface that gathers and presents weather
data without relying on a computer or smartphone.

The system is developed using **PlatformIO**, and the main functionality
includes: - Fetching and parsing weather data from SMHI
- Displaying temperature, precipitation, and forecast information
- Allowing user interaction via touchscreen menus
- Showing updated information at regular intervals

------------------------------------------------------------------------

## Getting started

This section explains how to set up the project on a new development
machine and prepare the ESP32.

### Prerequisites

Before you begin, make sure you have:

-   **Visual Studio Code** installed
-   **PlatformIO extension** installed in VS Code
-   An **ESP32 board**
-   A **compatible touchscreen display**
-   USB cable for flashing the microcontroller
-   Internet connection (required for fetching SMHI data)

### Installation

1.  Clone or download the project folder to your computer.
2.  Open the folder in **Visual Studio Code**.
3.  Ensure PlatformIO automatically detects the project.
4.  Install all required libraries (PlatformIO will usually prompt
    you).
5.  Connect your ESP32 to the computer via USB.

------------------------------------------------------------------------

## Building and running

Follow these instructions to build and run the system on the ESP32.

### Prepeare the device

Set device in download mode:
1. Press BOOT + RST
2. Release RST
3. Release BOOT

**You need to do this every time before you press UPLOAD**

<img src="./assets/screenshot.png" alt="Screenshot" width="500">

### Building and uploading

1.  In VS Code, open the PlatformIO sidebar.
2.  Select **Build** to compile the project.
3.  Select **Upload** to flash the compiled program to your ESP32.

### Startup procedure

-   Once powered on, the ESP32 connects to WiFi.
-   The system requests current weather and forecast data from the SMHI
    API.
-   The touchscreen interface initializes and shows the main menu or
    dashboard.

### Program operation

-   The system may include different screens such as:
    -   Current weather
    -   Hourly or daily forecast
    -   System settings
-   Users interact by tapping buttons and navigating menus on the
    touchscreen.
-   The device fetches new data at regular intervals or when manually
    refreshed.

------------------------------------------------------------------------

## Features

Below is a checklist of which **User Stories** have been implemented in
this project.
Adjust this list to match your actual progress.

-   [x]
-   [x]
-   [x] 
-   [ ]
-   [ ]
-   [ ] 
