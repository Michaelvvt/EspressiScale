# EspressiScale

EspressiScale is a minimalist and affordable espresso scale designed to provide accurate measurements along with a built-in timer – perfect for both home and small coffee shop use.

## Features

- **Accurate Weighing:** Provides precise measurements for your espresso brewing.
- **Built-in Timer:** Monitors brewing time to ensure optimal extraction.
- **Minimalist Design:** A simple and modern solution focusing on essential features.
- **Bluetooth Connectivity:** Connect to Gaggiuino or other compatible devices to stream weight and timer data.
- **Dual BLE Protocol Support:** Choose between native EspressiScale protocol or Acaia-compatible mode for wider app compatibility.

## Makerworld
This was also published on [makerworld](https://makerworld.com/en/models/1212476-espressiscale-small-minimalist-espresso-scale#profileId-1227630) for easy 3D printing.

## Bill of Materials (BOM)

| Quantity | Component                   | Details/Notes                                      |
|----------|-----------------------------|----------------------------------------------------|
| 1        | [Lilygo T-Touch bar](https://lilygo.cc/products/t-touch-bar?variant=42880705102005)          |                                                    |
| 1        | Battery                     | Any 3.7v rechargeable lithium battery (<10x34x48)       |
| 1        | [500g Load Cell](https://www.aliexpress.com/item/1005006390450639.html?spm=a2g0o.productlist.main.1.140c3ad4i723T1&algo_pvid=c0719cfc-035d-4043-bfff-a19c34d26db9&algo_exp_id=c0719cfc-035d-4043-bfff-a19c34d26db9-0&pdp_ext_f=%7B%22order%22%3A%2214%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%213.13%211.24%21%21%2124.03%219.53%21%40211b61bb17420425268903777eb948%2112000036996330117%21sea%21NO%210%21ABX&curPageLogUid=1wjYKMejCK7S&utparam-url=scene%3Asearch%7Cquery_from%3A)            |                                                    |
| 1        | [HX711](https://www.aliexpress.com/item/1005006293368575.html?spm=a2g0o.productlist.main.19.4b5474a6I3ualW&algo_pvid=8b13f76d-943b-4f75-8bf8-1741b29a8fbf&algo_exp_id=8b13f76d-943b-4f75-8bf8-1741b29a8fbf-9&pdp_ext_f=%7B%22order%22%3A%22487%22%2C%22eval%22%3A%221%22%7D&pdp_npi=4%40dis%21EUR%212.05%210.93%21%21%2115.75%217.18%21%40211b430817420419468811798eb8b9%2112000036639761167%21sea%21NO%210%21ABX&curPageLogUid=WayeqeRbhY4T&utparam-url=scene%3Asearch%7Cquery_from%3A)                     | High precision (available on AliExpress)           |
|1         | [USB type C port](https://a.aliexpress.com/_EyspPbK)             | Not yet implimented
| 4        | [M3x3.0 Threaded Inserts](https://cnckitchen.store/products/heat-set-insert-m3-x-3-short-version-100-pieces)     | (I used CNCkitchen)                                |
| 2        | M3x10 Screws                |                                                    |
| 2        | M3x12 Screws                |                                                    |
| 2        | M3x12 Countersunk Screws    |                                                    |
| -        | Heat Shrink                 |                                                    |

### Optional Components

| Quantity | Component                   | Details/Notes                                      |
|----------|-----------------------------|----------------------------------------------------|
| 2        | Extra Heated Inserts        |                                                    |
| 2        | M3x6 Screws                 |                                                    |



## Installation

1. **PlatformIO setup in Visual Studio:**

   Make sure you have PlatformIO installed in Visual Studio. You'll also need espressif platform installed.
   
3. **Clone the repository:**

   ```bash
   git clone https://github.com/whauu/EspressiScale.git

4. **Navigate to the project folder**

   ```bash
   cd EspressiScale

5. **Open project in PlatformIO**
   
6. **Erase flash**
   
7. **Build filesystem**
   
8. **Upload filesystem**
   
9. **Build**

10. **Upload**



## Usage
**Touch controls:**
- **Left Display:** Starts and stops timer
- **Right Display:** Tares weight and resets timer
- **Long Press (3 seconds):** Toggles between EspressiScale and Acaia-compatible BLE mode
  
**Power:**
  - Touch the display anywhere to wake it up
  - The scale will automatically enter deep sleep after 5min with no use

**Bluetooth:**
  - **EspressiScale mode:** The scale advertises as "EspressiScale" via Bluetooth
    - Compatible with Gaggiuino using the esp-arduino-ble-scales library
    - You can remotely tare the scale, start/stop/reset the timer, and receive weight and timer data
  - **Acaia-compatible mode:** The scale advertises as "Acaia" and mimics Acaia scale BLE protocol
    - Compatible with apps that support Acaia scales like Fellow, Artisan, and others
    - Provides the same functionality (weight, timer, tare) using the widely-supported Acaia protocol

**WiFi Configuration:**
  - EspressiScale creates a WiFi access point named "EspressiScale"
  - Default password: `Espress1Scale`
  - Connect to this network and your device should be redirected to a configuration portal
  - If not automatically redirected, open a browser and navigate to `192.168.4.1`
  - Follow the prompts to connect EspressiScale to your home WiFi network
  - Once connected, the scale's IP address will be displayed in the serial monitor

**Firmware Updates:**
  - After connecting EspressiScale to your WiFi network, access the update page by navigating to:
    `http://[scale-ip]/update` in your browser
  - Use this page to upload new firmware (.bin files)

**Update:**
   - Update using "scaleIP"/update
   - Build updated project
   - Upload the firmware.bin file
     <p align="center">
<img src="https://github.com/user-attachments/assets/cd5b869b-26ec-4d18-aa3c-554c78ec7306" alt="Screenshot" />
</p>

[PrettyOTA](https://github.com/LostInCompilation/PrettyOTA.git)

## Gaggiuino Integration

To integrate with Gaggiuino:

1. Make sure your Gaggiuino firmware includes the [esp-arduino-ble-scales](https://github.com/kstam/esp-arduino-ble-scales) library
2. Add the EspressiScale plugin to your Gaggiuino project by copying:
   - `esp-arduino-ble-scales/src/scales/espressiscale.h`
   - `esp-arduino-ble-scales/src/scales/espressiscale.cpp`
3. Add the following to your Gaggiuino initialization code:
   ```cpp
   #include "scales/espressiscale.h"
   
   void setup() {
     // ... other initialization code
     
     // Register the EspressiScale plugin
     EspressiScalesPlugin::apply();
     
     // ... other setup code
   }
   ```
4. Now your Gaggiuino should be able to automatically discover and connect to your EspressiScale

## License
This project is licensed under the MIT License

## Contact
If you have any questions or suggestions, you can:
- Open an issue in the GitHub repository
- Contact me directly at [whauu@espressiscale.com]

## BLE Protocol Modes

EspressiScale supports two different Bluetooth Low Energy (BLE) protocol modes:

1. **EspressiScale Native Protocol:**
   - Default mode with custom UUIDs
   - Designed specifically for EspressiScale
   - Compatible with Gaggiuino and other systems that implement the EspressiScale protocol

2. **Acaia-compatible Protocol:**
   - Mimics the protocol used by Acaia scales
   - Compatible with a wide range of existing coffee apps and equipment
   - Provides immediate compatibility with apps like Fellow, Artisan, and many others

### Switching Between Protocols

To switch between BLE protocols:
1. Long press on the scale display for 3 seconds
2. The display will show the new protocol mode ("BLE Mode: EspressiScale" or "BLE Mode: Acaia")
3. The scale will reconnect with the new protocol mode
4. Connect your app to the scale (it will appear as "EspressiScale" or "Acaia" depending on mode)

### Acaia App Compatibility

When in Acaia-compatible mode, EspressiScale should work with most apps that support Acaia scales, including:
- Fellow (iOS, Android)
- Artisan Roaster Scope
- Brewmaster
- Many espresso machine companion apps

**Note:** This compatibility mode is provided for convenience. For the best experience with dedicated Acaia apps, consider using a genuine Acaia scale.

## Troubleshooting

**WiFi Connection Issues:**
- If you cannot connect to your WiFi network, the scale will remain in access point mode for 5 minutes
- Connect to the "EspressiScale" network (password: `Espress1Scale`) to try again
- If you need to reset the WiFi configuration, restart the scale
