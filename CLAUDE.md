# Drillzilla

PID-controlled drill press: ESP32-S3 CYD + Nidec M200 VFD + 2HP 3-phase motor.

## Toolchain

- **PlatformIO** (not Arduino IDE)
- Board: ESP32-8048S050C (ESP32-S3, 8M PSRAM, 16M Flash)
- Framework: Arduino

## Hardware: ESP32-8048S050C (CYD)

5" IPS 800×480 RGB LCD, capacitive touch (GT911), portrait orientation (480×800).

### Display — ST7262 parallel RGB panel
- Driver: Arduino_GFX + Arduino_ESP32RGBPanel (not TFT_eSPI — SPI TFT libraries do not work)
- UI framework: LVGL 8.3.x
- Pixel clock: 16 MHz
- Color: 16-bit RGB565

### Display pin assignments (do not change)
- DE: 40, VSYNC: 41, HSYNC: 39, PCLK: 42
- R: 45, 48, 47, 21, 14
- G: 5, 6, 7, 15, 16, 4
- B: 8, 3, 46, 9, 1
- Backlight: GPIO 2

### Touch — GT911 (I2C)
- SDA: 19, SCL: 20, RST: 38
- Library: TAMC_GT911

### I2S Audio (speaker header)
- DOUT: 17, BCLK: 0, LRC: 18

### SD Card (SPI)
- SCK: 12, MISO: 13, MOSI: 11, CS: 10

### Extended IO header
Connector "SH-1.0 Extended IO" and "JS-1.25 Extended IO" break out spare GPIOs. These are used for RS485, encoder, tach, buttons, and LED. Check schematic for exact pinout before assigning.

## Architecture

- PCNT on ESP32-S3 uses `driver/pulse_cnt.h` (new API), not the legacy `driver/pcnt.h`
- RS485 Modbus RTU to VFD (38400 baud, 8-N-1)
- Rotary encoder (Grayhill 62A22) for speed setpoint
- Physical start/stop buttons — touchscreen never starts the motor
- Status LED for at-speed indication

## UI Design

- Portrait orientation (480 wide × 800 tall)
- LVGL-based touchscreen UI
- Main screen: large RPM readout, direction, load bar, speed presets
- Numpad for direct speed entry
- Settings screen for PID tuning, ramp times, presets editor
- See DRILLZILLA.md "Touchscreen UI" section for full spec

## Project Structure

- `DRILLZILLA.md` — full system spec (hardware, wiring, PID, state machine, UI)
- `drillzilla.ino` — legacy sketch for old CYD (not used, kept for reference)
- `5.0inch_ESP32-8048S050/` — vendor demo code and datasheets
