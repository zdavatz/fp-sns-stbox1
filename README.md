# FP-SNS-STBOX1 Firmware Package

![latest tag](https://img.shields.io/github/v/tag/STMicroelectronics/fp-sns-stbox1.svg?color=brightgreen)

The FP-SNS-STBOX1 is STM32Cube Function Pack for:
- SensorTile.box PRO discovery box with multi-sensors and wireless connectivity (STEVAL-MKBOXPRO) 
- STWIN.BOX Industrial Node Development Kit (STEVAL-STWINBX1) 
The purpose of this functional pack is to provide simple applications and examples that show how to build custom applications for STEVAL-MKBOXPRO Pro Mode and STEVAL-STWINBX1.

The expansion is built on STM32Cube software technology to ease portability across different STM32 microcontrollers.

**FP-SNS-STBOX1 software features**:

- Complete examples and applications to develop node with BLE connectivity, analog microphone, environmental and motion sensors, and perform real-time monitoring of sensors and audio data
- Example of how create one Boot Loader and one application for allowing Firmware Over the Air update
- Firmware compatible with ST BLE Sensor applications for Android/iOS, to perform sensor data reading, motion algorithm features demo and firmware update (FOTA)
- Easy portability across different MCU families, thanks to STM32Cube
- Free, user-friendly license terms

This firmware package includes Components Device Drivers, Board Support Package and example application for the:

- STMicroelectronics STEVAL-MKBOXPRO (SensorTile.box-Pro)  evaluation board that contains the following components:
  - MEMS sensor devices: STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
  - Dynamic NFC tag: ST25DV04K (board rev. A) - ST25DV64KC (board rev. B and board rev. C)
  - Digital Microphone: MP23db01HP
  
- STMicroelectronics STEVAL-STWINBX1 (STWIN.BOX) evaluation board that contains the following components:
  - MEMS sensor devices: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX,IIS2ICLX,ILPS22QS,STTS22H
  - analog/digital microphone 
  - ST25dv 64K
  - BlueNRG-2 Bluetooth Low Energy System On Chip

[![The FP-SNS-STBOX1 package contents](_htmresc/FP-SNS-STBOX1_Software_Architecture.png)]()

Here is the list of references to user documents:

- [DB4008: STM32Cube function pack for the Pro Mode of the SensorTile.box wireless multi sensor development kit ](https://www.st.com/resource/en/data_brief/fp-sns-STBOX1.pdf)
- [UM2626: Getting started with the STM32Cube function pack for the Pro Mode of the SensorTile.box wireless multi sensor development kit](https://www.st.com/resource/en/user_manual/um2626-getting-started-with-the-stm32cube-function-pack-for-the-pro-mode-of-the-sensortilebox-wireless-multi-sensor-development-kit-stmicroelectronics.pdf)
- [FP-SNS-STBOX1 Quick Start Guide](https://www.st.com/content/ccc/resource/sales_and_marketing/presentation/product_presentation/group0/5c/4e/96/c2/a6/98/4a/7f/FP-SNS-STBOX1_Quick_Start_Guide/files/FP-SNS-STBOX1_Quick_Start_Guide.pdf/jcr:content/translations/en.FP-SNS-STBOX1_Quick_Start_Guide.pdf)

## Supported Devices and Boards

- STEVAL-MKBOXPRO (SensorTile.box PRO) discovery box with multi-sensors and wireless connectivity for any intelligent IoT node\[[STEVAL-MKBOXPRO](https://www.st.com/en/evaluation-tools/steval-mkboxpro.html)\]
- STEVAL-STWINBX1 (STWIN.box) SensorTile Wireless Industrial Node Development Kit \[[STEVAL-STWINBX1](https://www.st.com/en/evaluation-tools/steval-stwinbx1.html)\]

## SD Card Data Format (SDDataLogFileX)

Logging starts automatically on power-on. Press the user button to stop, press again to restart. Each session creates two files on the SD card:

### Sensor CSV: `SensNNN.csv`

CSV at ~100 Hz with header:

```
Time [mS], AccX [mg], AccY [mg], AccZ [mg], GyroX [mdps], GyroY [mdps], GyroZ [mdps],MagX [mgauss],MagY [mgauss],MagZ [mgauss],P [mB],T ['C]
```

| Column | Unit | Sensor |
|--------|------|--------|
| Time | milliseconds (ThreadX tick) | System |
| AccX/Y/Z | milli-g | LSM6DSV16X (MKBOXPRO) / ISM330DHCX (STWINBX1) |
| GyroX/Y/Z | milli-degrees/sec | LSM6DSV16X / ISM330DHCX |
| MagX/Y/Z | milli-gauss | LIS2MDL (MKBOXPRO) / IIS2MDC (STWINBX1) |
| P | millibar (hPa) | LPS22DF (MKBOXPRO) / ILPS22QS (STWINBX1) |
| T | degrees Celsius | STTS22H |

### Audio WAV: `MicNNN.wav`

Standard RIFF WAV — mono, 16-bit PCM, 16 kHz sample rate (MP23DB01HP microphone). The file number `NNN` matches the corresponding sensor CSV.

## Visualization Scripts

Python scripts for plotting sensor and quaternion data are in `Utilities/scripts/`:

- **`visualize_sensors.py`** — Plot sensor CSV data (accelerometer, gyroscope, magnetometer, temperature, pressure) and optionally quaternion orientation data.
  ```
  python Utilities/scripts/visualize_sensors.py sensor_data.csv [quaternion_data.csv] [-o OUTPUT_DIR]
  ```
- **`visualize_pumpfoil.py`** — Pumpfoil session analysis: pump cadence spectrogram and movement phase detection.
  ```
  python Utilities/scripts/visualize_pumpfoil.py sensor_data.csv [-o OUTPUT_DIR]
  ```
- **`animate_board_3d.py`** — Animated board side-view GIF per session from quaternion data, with optional combined side-by-side MOV output synchronized with camera footage.
  ```
  # GIF only (all sessions)
  python Utilities/scripts/animate_board_3d.py quaternion_data.csv -o gif/

  # Single session GIF
  python Utilities/scripts/animate_board_3d.py quaternion_data.csv -o gif/ --session 1

  # Combined MOV with camera video and title card
  python Utilities/scripts/animate_board_3d.py quaternion_data.csv -o mov/ \
    --video camera.MOV --video-offset 56 --sensor-offset 1.5 \
    --session 1 --title "PumpGraph Mirco" --subtitle "ONIX Albatross 1160"
  ```
  Options:
  | Flag | Description |
  |------|-------------|
  | `-o` | Output directory (default: `.`) |
  | `--fps` | Frames per second (default: 15) |
  | `--session N` | Generate only session N (default: all) |
  | `--video FILE` | Camera video (MOV/MP4) for combined side-by-side MOV |
  | `--video-offset SEC` | Start time in camera video in seconds |
  | `--sensor-offset SEC` | Start time in sensor GIF in seconds |
  | `--title TEXT` | Title card text (green, bold) |
  | `--subtitle TEXT` | Subtitle card text (light blue, bold) |

  The animation automatically detects the steepest board angle during the drop-in (first 10s) and flashes **"Dropwinkel: X.X°"** in bold red, fading out over 2 seconds.

Generated plots are saved to the `png/` directory with filenames derived from the input CSV (e.g. `plot_quaternions_mirco_7.3.2026.png`).
Board animations are saved to `gif/` as `anim_board_*_sessionN.gif`.
Combined video+sensor side-by-side outputs (synced camera footage with board animation) are in `mov/` as MOV files for pause/scrub playback. When `--title`/`--subtitle` are provided, a 2-second title card is prepended.
Quaternion plots display time in min:sek format based on a 120 Hz sample rate.
Pumpfoil sessions are auto-detected (rhythmic pitch oscillation > 0.3 Hz filters out walking).
Each session plot includes quaternion components, Euler angles, and board nose angle relative to the water surface. The nose angle is computed from the rotated sensor Y-axis (sensor mounted in Breitachse = X across board), with a Butterworth 2 Hz low-pass filter (preserves ~1 Hz pump oscillation, removes high-frequency noise) and a 10-second centered rolling median baseline (tracks rider position, removes sensor drift). The board visualization shows both tilt and vertical translation proportional to the nose angle. Drop-in (green dashed line) and end-of-session crash (red shaded area) are marked.
The combined nose angle comparison plot (`plot_nose_angle_*.png`) includes FFT frequency analysis per session to distinguish pump frequency (~1 Hz) from magnetometer drift (~0.1 Hz).

**Sensor mounting note:** The carbon mast does not affect the magnetometer, but small metal screws near the sensor box cause hard/soft iron interference. Use plastic screws or adhesive tape instead. Data quality degrades in later sessions without sensor restart (gyroscope bias drift, stale magnetometer calibration). Restart the sensor between runs for best results.

## Magnetometer Firmware Improvements

The LIS2MDL magnetometer driver (`Drivers/BSP/Components/lis2mdl/lis2mdl.c`) has been modified to reduce drift during long sessions:

1. **Offset cancellation every ODR cycle** — `LIS2MDL_SENS_OFF_CANC_EVERY_ODR` continuously corrects sensor offsets instead of only at power-on
2. **Temperature compensation** — compensates for magnetometer offset drift caused by temperature changes during operation
3. **Low-pass filter at ODR/4** — 25 Hz bandwidth at 100 Hz ODR, filters high-frequency noise for cleaner signal

### Command-Line Build

Makefiles are provided for building firmware without STM32CubeIDE:

```
cd Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLESensorsPnPL/STM32CubeIDE
make

cd Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/STM32CubeIDE
make
```

Requires ARM GNU Toolchain (`arm-none-eabi-gcc`). Output binaries are in `build/`.

The toolchain path is configured in `config.mk` at the repository root:
```makefile
TOOLCHAIN = $(HOME)/.software/arm-gnu-toolchain/bin
```
Override per invocation: `make TOOLCHAIN=/other/path`.

Requires: `pandas`, `numpy`, `matplotlib`, `scipy`.

## Known Limitations

With some Android phones there are some compatibility issues for:

- using the PIN for BLE security connection (BLEDualProgram application). In this case disable the STBOX1_BLE_SECURE_CONNECTION on respective Inc/STBOX1_config.h file

- for forcing a full BLE rescan (BLEMLC). In this case disable the BLE_FORCE_RESCAN on their Inc/STBOX1_config.h files

In all these situations, before to connect to the SensorTile.box PRO, each time you change the running application, clean the Device Cache for forcing a rescan of BLE services.

## Development Toolchains and Compilers

-   IAR Embedded Workbench for ARM (EWARM) toolchain V9.60.3 + STLink/V2 or STLink/V3
-   RealView Microcontroller Development Kit (MDK-ARM) toolchain V5.38.0 + ST-LINK/V2 or STLink/V3
-   Integrated Development Environment for STM32 (STM32CubeIDE) V1.18.1 + ST-LINK or STLink/V3
	
## Dependencies 

This software release is compatible with:

- [**ST BLE Sensor Android application**](https://play.google.com/store/apps/details?id=com.st.bluems)  V5.0.0 (or higher)
- [**ST BLE Sensor iOS application**](https://apps.apple.com/it/app/st-ble-sensor/id993670214)  V5.0.0 (or higher)
