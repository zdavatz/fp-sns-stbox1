## <b>NFC_FTM Application Description</b>

This firmware package includes Components Device Drivers, Board Support Package and example application for the following STMicroelectronics elements:

  - STEVAL-STWINBX1 (STWIN.box) evaluation board that contains the following components:
      - MEMS sensor devices: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H
	  - Dynamic NFC tag: ST25DV64K
	  - On-board Bluetooth® low energy wireless technology: BlueNRG-M2
	  - Analog MEMS microphone: IMP23ABSU
	  - Industrial grade digital MEMS microphone: IMP34DT05 
 
The Example application provides one example on how to use the Fast Memory Transfer protocol for making a firmware update using the NFC 
and using the STMicroelectronics "ST25 NFC Tap" available for:

 - [Android](https://play.google.com/store/apps/details?id=com.st.st25nfc)
 - [iOS](https://apps.apple.com/be/app/nfc-tap/id12789135970)

Pressing the user button is possibile to enable/disable the Fast Transfer Memory process

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

NFC, SPI, I2C, UART, MEMS, BLE, BLE_Manager, BlueNRG-2

### <b>Hardware and Software environment</b>

- This example runs on STEVAL-STWINBX1 (STWIN.box) evaluation board and it can be easily tailored to any other supported device and development board.
- This example must be used with the related "ST25 NFC Tap" Android/iOS application available on the Google Play or Apple App Store, in order to read the sent information by Bluetooth Low Energy protocol

ADDITIONAL_COMP : [IIS2DLPC](https://www.st.com/en/mems-and-sensors/iis2dlpc.html)

ADDITIONAL_COMP : [IIS2MDC](https://www.st.com/en/mems-and-sensors/iis2mdc.html)

ADDITIONAL_COMP : [IIS3DWB](https://www.st.com/en/mems-and-sensors/iis3dwb.html)

ADDITIONAL_COMP : [ISM330DHCX](https://www.st.com/en/mems-and-sensors/ism330dhcx.html)

ADDITIONAL_COMP : [IIS2ICLX](https://www.st.com/en/mems-and-sensors/iis2iclx.html)

ADDITIONAL_COMP : [ILPS22QS](https://www.st.com/en/mems-and-sensors/ilps22qs.html)

ADDITIONAL_COMP : [STTS22H](https://www.st.com/en/mems-and-sensors/stts22h.html)

ADDITIONAL_COMP : [ST25DV64K](https://www.st.com/en/nfc/st25dv64k.html)

ADDITIONAL_COMP : [BlueNRG-M2](https://www.st.com/en/wireless-connectivity/bluenrg-m2.html)

ADDITIONAL_COMP : [IMP23ABSU](https://www.st.com/en/mems-and-sensors/imp23absu.html)

ADDITIONAL_COMP : [IMP34DT05](https://www.st.com/en/mems-and-sensors/imp34dt05.html)

### <b>Dependencies</b>

STM32Cube packages:

  - STM32U5xx drivers from STM32CubeU5 V1.7.0
  
X-CUBE packages:

  - X-CUBE-MEMS1 V11.2.0
  - X-CUBE-NFC4 V3.0.0

STEVAL-MKSBOX1V1:

  - STEVAL-STWINBX1 V1.1.0

### <b>How to use it?</b>

This package contains projects for 3 IDEs viz- IAR, Keil µVision 5 and Integrated Development Environment for STM32.
In order to make the  program work, you must do the following:

 - WARNING: before opening the project with any toolchain be sure your folder
   installation path is not too in-depth since the toolchain may report errors
   after building.

For IAR:

 - Open IAR toolchain (this firmware has been successfully tested with Embedded Workbench V9.60.3).
 - Open the IAR project file EWARM\BLELowPower.eww
 - Rebuild all files and Flash the binary on STEVAL-STWINBX1

For Keil µVision 5:

 - Open Keil µVision 5 toolchain (this firmware has been successfully tested with MDK-ARM Professional Version: 5.38.0).
 - Open the µVision project file MDK-ARM\Project.uvprojx
 - Rebuild all files and Flash the binary on STEVAL-STWINBX1
		
For Integrated Development Environment for STM32:

 - Open STM32CubeIDE (this firmware has been successfully tested with Version 1.18.1)
 - Set the default workspace proposed by the IDE (please be sure that there are not spaces in the workspace path).
 - Press "File" -> "Import" -> "Existing Projects into Workspace"; press "Browse" in the "Select root directory" and choose the path where the STM32CubeIDE project is located (it should be STM32CubeIDE\).
 - Rebuild all files and and Flash the binary on STEVAL-STWINBX1
   
### <b>Author</b>

SRA Application Team

### <b>License</b>

Copyright (c) 2025 STMicroelectronics.
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file
in the root directory of this software component.
If no LICENSE file comes with this software, it is provided AS-IS.
