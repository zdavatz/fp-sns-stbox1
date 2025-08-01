## <b>BLEPiano Application Description</b>

This firmware package includes Components Device Drivers, Board Support Package and example application for the following STMicroelectronics elements:

  - STEVAL-MKBOXPRO Rev C (SensorTile.box-Pro)  evaluation board that contains the following components:
      - MEMS sensor devices: STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
	  - Dynamic NFC tag: ST25DV64KC
	  - On-board Bluetooth® Low Energy: STM32WB07KC
      - Digital Microphone: MP23db01HP
 
The application explains how using bluetooth it is possible to play Music Notes on .box-Pro
The application provided also firwmare over the air update allowing also to change the Firwmare running on the board

### <b>Required STM32CubeMX settings</b>

Before code generation in Project Manager:

 - for toolchain/IDE EWARM set Min Version to 8.50 (default is 9.20).
 - for toolchain/IDE MKD-ARM set Min Version to 5.32 (default is 5.39).
 
Without this setting the code will be building but the flashing on board will fail.
 
### <b>Required IDE settings</b>

For Keil IDE:

 - set the "Micro LIB" option from within the "Project/Option for Target" menu (Target tab).
 - set the "Misc Controls" option with the "-Wno-format" string, from within the "Project/Option for Target" menu (C/C++ (AC6) tab).
 
### <b>Keywords</b>

NFC, SPI, I2C, UART, MEMS, BLE, BLE_Manager, STM32WB07KC

### <b>Hardware and Software environment</b>

- This example runs on STEVAL-MKBOXPRO (SensorTile.box-Pro) evaluation board and it can be easily tailored to any other supported device and development board.
- This example must be used with the related ST BLE Sensor Android/iOS application (Version 5.0.0 or higher) available on the Google Play or Apple App Store, in order to read the sent information by Bluetooth Low Energy protocol

ADDITIONAL_COMP : [STTS22H](https://www.st.com/en/mems-and-sensors/stts22h.html)

ADDITIONAL_COMP : [LPS22DF](https://www.st.com/en/mems-and-sensors/lps22df.html)

ADDITIONAL_COMP : [LSM6DSV16X](https://www.st.com/en/mems-and-sensors/lsm6dsv16x.html)

ADDITIONAL_COMP : [LIS2DU12](https://www.st.com/en/mems-and-sensors/lis2du12.html)

ADDITIONAL_COMP : [LIS2MDL](https://www.st.com/content/st_com/en/products/mems-and-sensors/e-compasses/lis2mdl.html)

ADDITIONAL_COMP : [MP23DB01HP](https://www.st.com/en/mems-and-sensors/mp23db01hp.html)

ADDITIONAL_COMP : [ST25DV64KC](https://www.st.com/en/nfc/st25dv64kc.html)

ADDITIONAL_COMP : [STM32WB07KC](https://www.st.com/en/microcontrollers-microprocessors/stm32wb07kc.html)

### <b>Dependencies</b>

STM32Cube packages:

  - STM32U5xx drivers from STM32CubeU5 V1.7.0
  
X-CUBE packages:

  - X-CUBE-BLEMGR V4.1.0
  - X-CUBE-MEMS1 V11.2.0
  - X-CUBE-NFC4 V3.0.0
  - X-CUBE-NFC7 V2.0.0
  
STEVAL-MKBOXPRO:

  - STEVAL-MKBOXPRO V1.5.0
	
### <b>How to use it?</b>

This package contains projects for 3 IDEs viz- IAR, Keil µVision 5 and Integrated Development Environment for STM32.
In order to make the  program work, you must do the following:

 - WARNING: before opening the project with any toolchain be sure your folder
   installation path is not too in-depth since the toolchain may report errors
   after building.

For IAR:

 - Open IAR toolchain (this firmware has been successfully tested with Embedded Workbench V9.60.3).
 - Open the IAR project file on EWARM directory
 - Rebuild all files and Flash the binary on STEVAL-MKBOXPRO

For Keil µVision 5:

 - Open Keil µVision 5 toolchain (this firmware has been successfully tested with MDK-ARM Professional Version: 5.38.0).
 - Open the µVision project file on MDK-ARM directory
 - Rebuild all files and Flash the binary on STEVAL-MKBOXPRO
		
For Integrated Development Environment for STM32:

 - Open STM32CubeIDE (this firmware has been successfully tested with Version 1.18.1)
 - Set the default workspace proposed by the IDE (please be sure that there are not spaces in the workspace path).
 - Press "File" -> "Import" -> "Existing Projects into Workspace"; press "Browse" in the "Select root directory" and choose the path where the STM32CubeIDE project is located (it should be STM32CubeIDE\).
 - Rebuild all files and and Flash the binary on STEVAL-MKBOXPRO
   
### <b>Author</b>

SRA Application Team

### <b>License</b>

Copyright (c) 2025 STMicroelectronics.
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file
in the root directory of this software component.
If no LICENSE file comes with this software, it is provided AS-IS.
