#include <algorithm>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstring>
#include <unistd.h>
#include <png.h>
#include "CL/opencl.h"
#include "PNGimg.h"

// prototype
bool init(int* data, size_t datasize);
void cleanup();
cl_platform_id findPlatform(const char *platformName);
cl_device_id findDevices(cl_platform_id pid);
cl_context createContext(cl_device_id dID);
cl_command_queue createQueue(cl_context ctx, cl_device_id dID);
cl_mem createBuffer(cl_context ctx, size_t size, int* data);
cl_program createProgram(cl_context ctx, cl_device_id dID);
cl_kernel createKernel(cl_program prog, char *kernel_name);
void readData(int* data, size_t size);
int checkErr(cl_int status, const char *errmsg);

// openCL variables
cl_int status; // for error code
cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_context context = NULL;
cl_command_queue queue = NULL;
cl_mem buffer = NULL;
cl_kernel kernel =NULL;
cl_program program = NULL;	


int main(){
	// begin main
	printf("YOLO world\n");

	// image paremeters
	int width;
	int height;
	int r,g,b;
	png_bytep *row_pointers;

	// Kernel var
	size_t datasize = 10 * sizeof(int);
	int data[10] = {0,1,2,3,4,5,6,7,8,9};

	size_t globalWorkSize[1];
	globalWorkSize[0] = 10;

	readData(data, datasize);
	
	openImg(&width, &height, &row_pointers);

	getRGBpixel(&r,&g,&b,width,height, row_pointers);
	process(width, height, row_pointers);
	write_png_file(width, height, row_pointers);

	png_bytep row = row_pointers[12];
	png_bytep px  = &(row[2 * 4]);

	printf("Got datas %d , %d\n", width, height);
	printf("2, 12 = RGBA(%3d, %3d, %3d, %3d)\n", px[0], px[1], px[2], px[3]);

	init(data, datasize);

	//loading kernel arguments
	printf("Loading kernel args\n");
	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer);
	checkErr(status, "Failed loading kernel args");

	// Executing kernel
	printf("Executing kernel\n");
	status = clEnqueueNDRangeKernel(
		queue, kernel, 1, NULL, globalWorkSize, NULL, 0, NULL,NULL);
	checkErr(status, "Failed executing kernel");

	//r Reading results
	printf("Reading results\n");
	status = clEnqueueReadBuffer(
			queue, buffer, CL_TRUE, 0, datasize, data, 0, NULL, NULL);
	checkErr(status, "Failed reading result from buffer");

	readData(data,datasize);

	printf("Cleaning up data (avoid memory leaks)\n");	
	cleanup();
	
	if(row_pointers){
		for(int y = 0; y < height ; y++){
			free(row_pointers[y]);
			row_pointers[y] = NULL;
		}
		free(row_pointers);
		row_pointers = NULL;
	}

	return 0;
}

bool init(int* data, size_t datasize){
	// find platform id
	platform = findPlatform("Altera");

	// find devices
	device = findDevices(platform);
	
	// create context
	context = createContext(device);

	// create command queue	
	queue = createQueue(context, device);	

	// create buffer and fill it with data
	buffer = createBuffer(context, datasize, data);
	
	// create program
	program = createProgram(context, device);

	// create kernel
	kernel = createKernel(program, "vec_double");
}

cl_platform_id findPlatform(const char *platformName){
	cl_uint num_platforms;

	printf("Getting number of openCL platform avialable\n");
	
	status = clGetPlatformIDs(0,NULL, &num_platforms);
	checkErr(status, "Failed getting number of openClPlatoform");

	cl_platform_id pIDs [num_platforms];

	printf("Getting all platforms IDs\n");
	
	// Get a list of all those platofrms IDs
	status = clGetPlatformIDs(num_platforms, pIDs, NULL);
	checkErr(status, "Failed retriving all platforms IDs");
	

	return pIDs[0];
}

cl_device_id findDevices(cl_platform_id pid){
	cl_uint num_devices;

	printf("Getting number of openCL devices\n");

	status = clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL ,0 , NULL, &num_devices);
	checkErr(status, "No AOCL devices found");

	cl_device_id dIDs [num_devices];

	printf("Retriving device ID\n");

	status = clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, num_devices, dIDs,NULL);
	checkErr(status, "Failed retriving device ID");
	
	return dIDs[0]; // only keep first one 
}

cl_context createContext(cl_device_id dID){
	cl_context ctx;

	printf("Create context\n");

	ctx = clCreateContext(NULL, 1, &dID, NULL, NULL, &status);
	checkErr(status,"Failed while creating context");

	return ctx;
}

cl_mem createBuffer(cl_context ctx, size_t size, int*data ){
	cl_mem buff;

	printf("Creating buffer\n");

	buff = clCreateBuffer(
		ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, size, data, &status);
	checkErr(status, "Failed while creating buffer");

	return buff ;
}

cl_command_queue createQueue(cl_context ctx, cl_device_id dID){
	cl_command_queue queue;

	printf("Create command queue\n");

	queue = clCreateCommandQueue(ctx, dID, CL_QUEUE_PROFILING_ENABLE, &status);
	checkErr(status, "Failed creating command queue");

	return queue;
}

cl_program createProgram(cl_context ctx, cl_device_id dID){
	cl_program prog;
	FILE* fp;
	size_t size;
	unsigned char* binary =  NULL;
	cl_int binaryStatus;
	
	printf("Looking for .aocx file\n");

	// Open file and get size
	fp = fopen("bin/kernel.aocx","r");

	if(fp == NULL){	printf("Cannot find AOCX file \n");} // TODO error handling	

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	// read .aocx
	binary = (unsigned char*)malloc(size + 1);  
	binary[size] = '\0'; // EOF char let's see if we really nned it 
	fread(binary, sizeof(char), size, fp);
	fclose(fp);
	fp = NULL;

	printf("Creating program\n");

	prog = clCreateProgramWithBinary(ctx, 1, &dID, &size,
					 (const unsigned char**)(&binary),
					&binaryStatus,
					&status);
	checkErr(status, "Failed creating program");
	free(binary); // free binary
	binary = NULL;

	printf("Building program\n");	

	status = clBuildProgram(prog, 0, NULL, "", NULL, NULL);
	checkErr(status, "Failed building program");

	return prog;
}

cl_kernel createKernel(cl_program prog, char *kernel_name){
	cl_kernel ker;
	
	printf("Creating kernel\n");
	
	ker = clCreateKernel(prog, kernel_name, &status);
	checkErr(status, "Failed building program");

	return ker;
}

void cleanup(){
	// here free all kernel , program , queue , context (in that order)
	
	if(kernel){
		clReleaseKernel(kernel);
		kernel = NULL;
	}	
	if(program){
		clReleaseProgram(program);
		program = NULL;
	}
	if(queue){
		clReleaseCommandQueue(queue);
		queue = NULL;
	}
	if(context){
		clReleaseContext(context);
		context = NULL;
	}
	if(buffer){
		clReleaseMemObject(buffer);
		buffer = NULL;
	}
}

void readData(int* data, size_t size){
	int i;
	printf("Data : \n");
	for(i = 0; i < (size/sizeof(int)); i++){
		printf("%d, ", data[i]);
	}
	printf("End of data\n");
}

int checkErr(cl_int status, const char *errmsg){
	if(status != CL_SUCCESS){
		printf("Operation failded : %s\nError number %d\n", errmsg, status);
		return -1;//TODO Make a true routine to quit in case of error
	}else{
		printf("SUCCESS\n");
		return 0;
	}
}

