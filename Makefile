AOCL_COMPILE_CONFIG=$(shell aocl compile-config)
AOCL_LINK_CONFIG=$(shell aocl link-config)

all: faces
faces : faces.o PNGimg.o
	g++ -o bin/faces faces.o PNGimg.o -L/home/amgarin/AOCL/altera/14.0/hld/linux64_13.1/lib/ $(AOCL_LINK_CONFIG) -lpng

faces.o : host/src/faces.cpp
	g++ -c host/src/faces.cpp $(AOCL_COMPILE_CONFIG)

PNGimg.o : host/src/PNGimg.cpp
	g++ -c host/src/PNGimg.cpp 

run : 
	CL_CONTEXT_EMULATOR_DEVICE_ALTERA=de1soc_sharedonly bin/faces

kernel: device/kernel.cl
	aoc -march=emulator --board de1soc_sharedonly device/kernel.cl -o bin/kernel.aocx

ARMfaces : ARMfaces.o ARMPNGimg.o
	arm-linux-gnueabihf-g++ -o binARM/faces ARMfaces.o ARMPNGimg.o -L/home/razoff/EPFL/bachelorProject/algoPDB/openCLonFPGA/lib $(AOCL_LINK_CONFIG) -lpng -pthread -lz 

ARMfaces.o : host/src/faces.cpp
	arm-linux-gnueabihf-g++ -c host/src/faces.cpp $(AOCL_COMPILE_CONFIG) -I/home/razoff/EPFL/bachelorProject/algoPDB/openCLonFPGA/include -o ARMfaces.o

ARMPNGimg.o : host/src/PNGimg.cpp
	arm-linux-gnueabihf-g++ -c host/src/PNGimg.cpp -I/home/razoff/EPFL/bachelorProject/algoPDB/openCLonFPGA/include -o ARMPNGimg.o

init: ~/init14.sh
	source ~/init14.sh

clean :
	rm *.o && rm bin/faces

cleanCL:
	rm *.aoco && rm -rf bin_kernel && rm bin/kernel.aocx 

cleanARM:
	rm binARM/*
