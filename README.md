# Temple Dash for ESP32 CYD

A fun Temple Run-inspired endless runner for the 2.4-inch ESP32-2432S024 (CYD) touchscreen. It uses simple shapes rather than image assets to keep memory use low.

## Hardware

- ESP32-2432S024, 240 × 320 TFT in landscape orientation (320 × 240)
- XPT2046 resistive touchscreen

## Controls

- Tap the left side to switch lanes left.
- Tap the right side to switch lanes right.
- Tap the middle area to jump.
- Tap to start or restart.

## Arduino IDE setup

1. Install the ESP32 board support package and select **ESP32 Dev Module**.
2. Install **TFT_eSPI** and **XPT2046_Touchscreen** from Library Manager.
3. Use the TFT_eSPI setup that is already configured for your particular CYD. This sketch expects that display setup to work and uses touch chip-select GPIO 33.
4. Open `TempleDash_CYD.ino`, select the correct port, and upload.

Touch calibration values are specific to the panel used during development. If your touches map to the wrong area, adjust `RAW_X_MIN`, `RAW_X_MAX`, `RAW_Y_MIN`, `RAW_Y_MAX`, and the axis/inversion flags near the top of the sketch.

## Notes

This is an independent hobby project inspired by the endless-runner genre; it is not affiliated with or a copy of the Temple Run game. The graphics are drawn from basic shapes. AI assistance was used during development; the sketch was adapted and debugged for the target display and touch hardware. The author should be prepared to explain the code and describe what was personally tested.

## License

No license is included. All rights remain with the author; public visibility alone does not grant permission to reuse or redistribute this code.
