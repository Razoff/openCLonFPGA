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
bool init();
void cleanup();
cl_platform_id findPlatform(const char *platformName);
cl_device_id findDevices(cl_platform_id pid);
cl_context createContext(cl_device_id dID);
cl_command_queue createQueue(cl_context ctx, cl_device_id dID);
cl_mem createRWBuffer(cl_context ctx, size_t size, int* data);
cl_mem createRBuffer(cl_context ctx, size_t size, int* data);
cl_mem createWBuffer(cl_context ctx, size_t size, int* data);
cl_program createProgram(cl_context ctx, cl_device_id dID);
cl_kernel createKernel(cl_program prog, char *kernel_name);
int checkErr(cl_int status, const char *errmsg);

// openCL variables
cl_int status; // for error code
cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_context context = NULL;
cl_command_queue queue = NULL;
cl_mem grey = NULL;
cl_mem red = NULL;
cl_mem green = NULL;
cl_mem blue = NULL;
cl_kernel kernel =NULL;
cl_program program = NULL;	


int main(){
	// begin main
	printf("YOLO world\n");

	// image paremeters
	int width;
	int height;
	int *r,*g,*b,*img;
	png_bytep *row_pointers;

	// Kernel var
	openImg(&width, &height, &row_pointers);

	size_t nb_pixel = width * height;
	size_t data_size = nb_pixel * sizeof(int);

	img = (int*)malloc(data_size); 

	size_t globalWorkSize[1];	
	globalWorkSize[0] = nb_pixel;
	
	getRGBpixel(&r,&g,&b,width,height, row_pointers);

	init();

	// Create buffers 
	red = 	createRBuffer(context, data_size, r);
	green = createRBuffer(context, data_size, g);
	blue = 	createRBuffer(context, data_size, b);
	grey =	createWBuffer(context, data_size, NULL);

	// Free rgb buffers
	free(r);
	free(g);
	free(b);

	//loading kernel arguments
	printf("Loading kernel args\n");
	printf("Red, ");
	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &red);
	printf("green, ");
	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &green);
	printf("blue, ");
	status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &blue);
	printf("grey\n");
	status = clSetKernelArg(kernel, 3, sizeof(cl_mem), &grey);

	checkErr(status, "Failed loading kernel args");
	
	// Executing kernel
	printf("Executing kernel\n");
	status = clEnqueueNDRangeKernel(
		queue, kernel, 1, NULL, globalWorkSize, NULL, 0, NULL,NULL);
	checkErr(status, "Failed executing kernel");

	// Reading results
	printf("Reading results\n");
	status = clEnqueueReadBuffer(
			queue, grey, CL_TRUE, 0, data_size, img, 0, NULL, NULL);
	checkErr(status, "Failed reading result from buffer");
	
	process(width, height, row_pointers, img);
	write_png_file(width, height, row_pointers);

	printf("Cleaning up data (avoid memory leaks)\n");	
	cleanup();

	free(img);	
	
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

bool init(){
	// find platform id
	platform = findPlatform("Altera");

	// find devices
	device = findDevices(platform);
	
	// create context
	context = createContext(device);

	// create command queue	
	queue = createQueue(context, device);	

	// create program
	program = createProgram(context, device);

	// create kernel
	kernel = createKernel(program, "grey_shade");
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

cl_mem createWRBuffer(cl_context ctx, size_t size, int*data){
	cl_mem buff;

	printf("Creating buffer\n");

	if(data != NULL){
		printf("Read / write with datas\n");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
			size, data, &status);
		checkErr(status, "Failed while creating buffer");
	}
	if(data == NULL){
		printf("Read / write without datas\n");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_WRITE,size, NULL, &status);
		checkErr(status, "Failed while creating buffer");
	}

	return buff ;
}
cl_mem createRBuffer(cl_context ctx, size_t size, int*data){
	cl_mem buff;

	printf("Creating buffer\n");

	if(data != NULL){
		printf("Read with datas\n");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			size, data, &status);
		checkErr(status, "Failed while creating buffer");
	}
	if(data  == NULL){
		printf("Read without datas\n");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_ONLY, size, NULL, &status);
		checkErr(status, "Failed while creating buffer");
	}

	return buff ;
}

cl_mem createWBuffer(cl_context ctx, size_t size, int*data){
	cl_mem buff;

	printf("Creating buffer\n");

	if(data != NULL){
		printf("Write with datas\n");
		buff = clCreateBuffer(
			ctx, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
			size, data, &status);
		checkErr(status, "Failed while creating buffer");
	}

	if(data == NULL){
		printf("Write without datas\n");
		buff = clCreateBuffer(
			ctx, CL_MEM_WRITE_ONLY, size, NULL, &status);
		checkErr(status, "Failed while creating buffer");
	}

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
	if(red){
		clReleaseMemObject(red);
		red = NULL;
	}
	if(green){
		clReleaseMemObject(green);
		green = NULL;
	}
	if(blue){
		clReleaseMemObject(blue);
		blue = NULL;
	}
	if(grey){
		clReleaseMemObject(grey);
		grey = NULL;
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

