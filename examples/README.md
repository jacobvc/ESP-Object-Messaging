# ESP-Object-Messaging Examples

These examples demonstrate incremental messaging functionality, all of which utilize the physical I/O of
the [Esp32IoAdapt](https://github.com/jacobvc/ESP32-Hardware-Boards/tree/main/Esp32IoAdapt) board.

Each increment is a formal superset of the previous example

## Esp32IoAdapt
Utilizes the [Esp32IoAdapt](https://github.com/jacobvc/ESP32-Hardware-Boards/tree/main/Esp32IoAdapt) board with 2 joysticks, 2 slider potentiometers, a servo, and two LEDs.  

## IoAdapt-Websock
This example adds HTTP and WebSocket server support, and a web page that produces and consumes the Esp32IoAdapt data

html / js files in html subdirectory, and EMBED_FILES directive in CMakeLists.txt, and supported by HttpPaths.cpp
## IoAdapt-Lvgl-Websock
This example adds a SquareLine-created LVGL touch screen display that produces and consumes the Esp32IoAdapt data

PYTHON TOOL: Produce / Consume
