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

intel: faces.o PNGimg.o
	g++ -o bin/faces faces.o PNGimg.o -L/opt/intel/opencl-sdk/lib64 -lpng -lOpenCL

clean :
	rm *.o && rm bin/faces

cleanCL:
	rm *.aoco && rm -rf bin_kernel && rm bin/kernel.aocx 

