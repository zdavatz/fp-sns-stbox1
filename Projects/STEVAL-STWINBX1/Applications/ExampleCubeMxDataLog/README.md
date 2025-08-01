## <b>ExampleCubeMxDataLog Application Description for STEVAL-STWINBX1 (STWIN.box) evaluation board</b>

This firmware package includes Components Device Drivers, Board Support Package and example application for the following STMicroelectronics elements:

  - STEVAL-STWINBX1 (STWIN.box) evaluation board that contains the following components:
      - MEMS sensor devices: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H
	  - ST25dv 64K
      - analog/digital microphone 
 
The Example application provides one CubeMX example of one simple Data Logger that reads the sensors' values and prints the output on one Serial port with the following configuration

- Speed: 115200
- Data: 8 bit
- Parity: None
- Stop Bits: 1 bit
- Flow control: None

### <b>Hardware and Software environment</b>

- This example runs on STEVAL-STWINBX1 (STWIN.box) evaluation board and it can be easily tailored to any other supported device and development board.
	
### <b>How to use it?</b>

This package contains one ioc file tested for CubeMX V6.9.2
double click on the .ioc file, with cubeMX select the target IDE for generating the code and project
Compile the generated project and executed on the STEVAL-STWINBX1 (STWIN.box) evaluation board

### <b>Author</b>

SRA Application Team

### <b>License</b>

Copyright (c) 2023 STMicroelectronics.
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file
in the root directory of this software component.
If no LICENSE file comes with this software, it is provided AS-IS.
