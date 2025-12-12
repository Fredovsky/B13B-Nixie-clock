# Nixie Tube Desk Clock

A beautiful 4-digit nixie tube clock powered by an ATmega328P microcontroller, featuring automatic brightness adjustment and anti-cathode poisoning routines.

## Features

- **4-Digit Time Display**: Shows hours and minutes in HH:MM format
- **Automatic Brightness Control**: Gradually adjusts brightness throughout the day based on time (no light sensor required)
- **Cathode Poisoning Prevention**: Automatic cleaning cycles to maintain nixie tube health
  - Short cycle every hour (15 seconds)
  - Long cycle daily at 3:00 AM (30 minutes)
- **Simple Time Setting**: Two-button interface (Hour/Minute) with auto-repeat on long press
- **RTC Backup**: DS3231 real-time clock with battery backup maintains accurate time
- **Visual Error Indicators**: Blinking display indicates RTC issues or power loss

## Version 1.1 Changes

This version (v1.1) includes several improvements over v1.0:

### Hardware Changes
- **Simplified button interface**: 2 buttons (Hour/Minute) instead of 3 (Mode/+/-)
- **Removed brightness sensor**: Automatic brightness adjustment is now time-based via software
- **Display mode**: Time only (no date/year display modes)
- **PCB redesign**: Now in KiCad format (previously Eagle)

### Software Improvements
- Restored and improved blinking functionality for error indication
- Removed blocking delays from button handling for better responsiveness
- Implemented proper auto-repeat for time adjustment buttons
- Added RTC communication error detection
- Cleaned up code structure with named constants
- Optimized SPI communication
- Fixed potential brightness range bugs

*Code cleaned and improved with assistance from Claude AI*

## Hardware Components

### Main Festures
- **Microcontroller**: ATmega328P
- **RTC Module**: DS3231 with CR2032 battery backup
- **Nixie Tubes**: 4× B31B socket tubes (10-digit tubes)
- **High Voltage Drivers**: 2× HV5530 (32-bit shift registers)

### Pin Assignments
```
Pin 6:  Hour button input
Pin 7:  Minute button input
Pin 9:  PWM output for brightness control
Pin 10: SPI latch pin for HV5530
Pin 11: SPI MOSI (to HV5530)
Pin 13: SPI SCK (to HV5530)
```

## Project Files

### PCB Design
The `PCB/` folder contains:
- KiCad project files (.kicad_pro, .kicad_sch, .kicad_pcb)
- Gerber files for PCB manufacturing
- Bill of Materials (BOM)

### 3D Printed Case
The `3D Files/` folder contains:
- STL files for case components
- CAD source files

### Firmware
- `main.cpp`: Arduino-compatible C++ code for ATmega328P
- Uses Arduino framework with RTClib library (v1.12.4)

## Software Configuration

### Brightness Settings
Automatic brightness adjustment can be customized:

```cpp
uint8_t minBrightness = 15;        // Minimum PWM (0-255)
uint8_t maxBrightness = 240;       // Maximum PWM (0-255)
uint8_t timeStartDecrease = 19;    // Hour to start dimming (24h format)
uint8_t timeStartIncrease = 6;     // Hour to start brightening (24h format)
```

**How it works:**
- Brightness gradually decreases from 7 PM (19:00) to 6 AM
- Brightness gradually increases from 6 AM to 7 PM
- Changes by 1 PWM unit per minute (~3.75 hours for full transition)

### Cathode Poisoning Prevention
```cpp
constexpr uint8_t CLEANING_HOUR = 3;  // Hour for long cleaning cycle (3 AM)
```

The clock automatically cycles through all digits to prevent cathode poisoning:
- **Short cycle**: Every hour on the hour (except at cleaning hour) - 15 seconds
- **Long cycle**: Daily at 3:00 AM - 30 minutes

## Building and Uploading

### Requirements
- PlatformIO
- USBasp, Arduino as ISP, or similar programmer for ATmega328P

### Setting Initial Time
If the RTC has lost power (battery dead or first-time setup):
1. The display will show `00:00` blinking
2. Use Hour and Minute buttons to set the correct time
3. Blinking stops once you press a button (confirms time set)

## Usage

### Normal Operation
- Display shows current time in HH:MM format
- Brightness adjusts automatically throughout the day
- Cathode cleaning cycles run automatically

### Setting Time
- **Short press Hour button**: Increment hour by 1
- **Long press Hour button**: Auto-repeat (fast increment)
- **Short press Minute button**: Increment minute by 1
- **Long press Minute button**: Auto-repeat (fast increment)

### Error Indicators
- **Blinking 88:88**: RTC communication error
- **Blinking 00:00**: RTC lost power (battery dead or removed - set time)

## Troubleshooting

### Display shows 00:00 blinking
- Replace CR2032 battery in DS3231 module
- Set time using Hour/Minute buttons

### Tubes don't light up
- Check high voltage power supply (~170V)
- Verify HV5530 connections and power
- Test SPI communication (MOSI, SCK, Latch)

### Time drifts
- DS3231 is very accurate (±2ppm)
- Check RTC battery voltage
- Ensure crystal is functioning

## License

MIT License

Copyright (c) 2025 Frederic Lidove

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

## Credits

- Hardware design: Frederic Lidove
- Firmware v1.1: Code improvements by Claude AI (Anthropic)

## Changelog

### v1.1 (Current)
- Simplified to 2-button interface (H/M only)
- Removed hardware brightness sensor
- Implemented software-based automatic brightness adjustment
- Removed date/year display modes (time only)
- PCB redesigned in KiCad
- Code cleanup and optimization
- Improved error handling and blinking indicators

### v1.0
- Initial release
- 3-button interface (Mode/+/-)
- Hardware brightness sensor
- Date and year display modes
- Eagle PCB design

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.
