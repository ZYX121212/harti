# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

Harti is a desktop interactive pet based on ESP32-S3, featuring a 240x240 GC9A01 round SPI LCD screen displaying animated expressive eyes.

---

## Common Commands

### Setup & Environment
```bash
# Source ESP-IDF environment (run in new terminals)
. ~/esp-idf/export.sh

# Set target chip
idf.py set-target esp32s3
```

### Build & Flash
```bash
# Build
idf.py build

# Flash and monitor
idf.py -p /dev/tty.usbmodem* flash monitor

# Clean
idf.py fullclean
```

---

## Architecture

### Directory Structure
```
harti/
├── main/              # Application logic
│   ├── main.c         # Entry point
│   └── app_display.c  # Emotion management & animation
├── components/
│   ├── gc9a01/        # GC9A01 LCD driver
│   └── expressive_eyes/  # Eye rendering engine
└── docs/              # Architecture & hardware docs
```

### Key Modules

| Module | Purpose | Key Files |
|--------|---------|-----------|
| `gc9a01` | Low-level SPI LCD driver | `components/gc9a01/gc9a01.{c,h}` |
| `expressive_eyes` | Low-memory scanline-based eye renderer | `components/expressive_eyes/expressive_eyes.{c,h}` |
| `app_display` | High-level emotion state machine & micro-animations | `main/app_display.{c,h}` |

### Rendering Pipeline

The display uses a **line-buffered, scanline-based rendering approach** for minimal memory usage (~480 bytes total):
1. Set full-screen window on GC9A01
2. For each scanline:
   - Render background (radial gradient)
   - Render left/right eyes with iris, pupil, eyelids
   - Render decorators (blush, tears, stars)
   - Send line to LCD via SPI

### Emotion System

Emotions are defined as parametric `eye_state_t` structs containing:
- Eye position & separation
- Lid openness
- Pupil position & scale
- Expression curves (up/down)
- Decorator levels (blush, tears, stars)

Preset emotions: `NEUTRAL`, `HAPPY`, `SAD`, `SURPRISED`, `SLEEPY`, `ANGRY`, `BORED`, `EXCITED`

---

## Hardware

- MCU: ESP32-S3 (N16R8, 16MB Flash, 8MB PSRAM)
- LCD: GC9A01 240x240 round SPI screen
- Sensor options: IMU (MPU6050/QMI8658), light sensor (BH1750), digital mic (INMP441)

See `docs/HARDWARE.md` for pinout.
