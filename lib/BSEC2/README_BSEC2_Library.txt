Note for usage of the Bosch Sensortec BSEC2 library:

The files already existing in lib/BSEC2 are neccessary for PIO regarding special build flags (because some parts of the lib are linked statically).
Since BSEC2 is not Open Source, you will have to download it at https://www.bosch-sensortec.com/software-tools/software/bme688-software/ .
Extract the the downloaded zip file and copy the src folder under examples/bsec2 to the BSEC folder in the PIO project.