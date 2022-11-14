Note for usage of the Bosch Sensortec BSEC2 library:

The files already existing in lib/BSEC2 are neccessary for PIO regarding special build flags (because some parts of the lib are linked statically).
Since BSEC2 is not open source, you will have to download it at https://www.bosch-sensortec.com/software-tools/software/bme688-software/ .
Extract the the downloaded zip file and copy the src folder from examples/bsec2 to the lib/BSEC2 folder in the PIO project.
The newer versions of BSEC2 depends also on a second Bosch library which can be downloaded here: https://github.com/BoschSensortec/Bosch-BME68x-Library/releases. Copy the src folder containing bme68x_Library.h to lib/bme68x_library.
This code is tested and works with BSEC2 version 2.2.0.0.