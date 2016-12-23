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
cl_mem createRWBuffer(cl_context ctx, size_t size, void* data);
cl_mem createRBuffer(cl_context ctx, size_t size, void* data);
cl_mem createWBuffer(cl_context ctx, size_t size, void* data);
cl_program createProgram(cl_context ctx, cl_device_id dID);
cl_kernel createKernel(cl_program prog, char* kernel_name);
void blackAndWhite(int* r, int* g, int* b, int** ret,
		size_t nb_pixel, size_t data_size);
void edgeD(int* gShades , int** sobel, int width, int height,
                size_t nb_pixel, size_t data_size);
void houghLine( int* sobel, int** houghL, int width, int height,
                size_t nb_pixel, size_t data_size);
int checkErr(cl_int status, const char *errmsg);

// openCL variables
cl_int status; // for error code
cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_context context = NULL;
cl_command_queue queue = NULL;
cl_program program = NULL;	

int main(){
	// begin main
	printf("YOLO world\n");

	// image paremeters
	int width;
	int height;
	int *r,*g,*b,*img,*sobel,*accumulator;
	png_bytep *row_pointers;

	// Kernel var
	openImg(&width, &height, &row_pointers);

	size_t nb_pixel = width * height;
	size_t data_size = nb_pixel * sizeof(int);

	getRGBpixel(&r,&g,&b,width,height, row_pointers);

	init();
	
	// apply kernel to output black and white png
	blackAndWhite(r, g, b, &img, nb_pixel, data_size);

	// edge detection
	edgeD(img, &sobel, width, height, nb_pixel, data_size);	

	// line detection
	houghLine(sobel,&accumulator, width, height, nb_pixel, data_size);
	
	process(width, height, row_pointers, sobel);
	write_png_file(width, height, row_pointers);

	printf("Cleaning up data (avoid memory leaks)\n");	
	cleanup();

	free(sobel);
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

}

cl_platform_id findPlatform(const char *platformName){
	cl_uint num_platforms;

	printf("Getting number of openCL platform avialable : ");
	
	status = clGetPlatformIDs(0,NULL, &num_platforms);
	checkErr(status, "Failed getting number of openClPlatoform");

	cl_platform_id pIDs [num_platforms];

	printf("Getting all platforms IDs : ");
	
	// Get a list of all those platofrms IDs
	status = clGetPlatformIDs(num_platforms, pIDs, NULL);
	checkErr(status, "Failed retriving all platforms IDs");
	

	return pIDs[0];
}

cl_device_id findDevices(cl_platform_id pid){
	cl_uint num_devices;

	printf("Getting number of openCL devices : ");

	status = clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL ,0 , NULL, &num_devices);
	checkErr(status, "No AOCL devices found");

	cl_device_id dIDs [num_devices];

	printf("Retriving device ID : ");

	status = clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, num_devices, dIDs,NULL);
	checkErr(status, "Failed retriving device ID");
	
	return dIDs[0]; // only keep first one 
}

cl_context createContext(cl_device_id dID){
	cl_context ctx;

	printf("Create context : ");

	ctx = clCreateContext(NULL, 1, &dID, NULL, NULL, &status);
	checkErr(status,"Failed while creating context");

	return ctx;
}

cl_mem createWRBuffer(cl_context ctx, size_t size, void*data){
	cl_mem buff;

	printf("Creating buffer of type ");

	if(data != NULL){
		printf("Read / write with datas : ");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
			size, data, &status);
		checkErr(status, "Failed while creating buffer");
	}
	if(data == NULL){
		printf("Read / write without datas : ");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_WRITE,size, NULL, &status);
		checkErr(status, "Failed while creating buffer");
	}

	return buff ;
}
cl_mem createRBuffer(cl_context ctx, size_t size, void*data){
	cl_mem buff;

	printf("Creating buffer of type ");

	if(data != NULL){
		printf("Read with datas : ");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			size, data, &status);
		checkErr(status, "Failed while creating buffer");
	}
	if(data  == NULL){
		printf("Read without datas : ");
		buff = clCreateBuffer(
			ctx, CL_MEM_READ_ONLY, size, NULL, &status);
		checkErr(status, "Failed while creating buffer");
	}

	return buff ;
}

cl_mem createWBuffer(cl_context ctx, size_t size, void*data){
	cl_mem buff;

	printf("Creating buffer of type ");

	if(data != NULL){
		printf("Write with datas : ");
		buff = clCreateBuffer(
			ctx, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
			size, data, &status);
		checkErr(status, "Failed while creating buffer");
	}

	if(data == NULL){
		printf("Write without datas : ");
		buff = clCreateBuffer(
			ctx, CL_MEM_WRITE_ONLY, size, NULL, &status);
		checkErr(status, "Failed while creating buffer");
	}

	return buff ;
}

cl_command_queue createQueue(cl_context ctx, cl_device_id dID){
	cl_command_queue queue;

	printf("Create command queue : ");

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

	printf("Creating program : ");

	prog = clCreateProgramWithBinary(ctx, 1, &dID, &size,
					 (const unsigned char**)(&binary),
					&binaryStatus,
					&status);
	checkErr(status, "Failed creating program");
	free(binary); // free binary
	binary = NULL;

	printf("Building program : ");	

	status = clBuildProgram(prog, 0, NULL, "", NULL, NULL);
	checkErr(status, "Failed building program");

	return prog;
}

cl_kernel createKernel(cl_program prog, char *kernel_name){
	cl_kernel ker;
	
	printf("Creating kernel : ");
	
	ker = clCreateKernel(prog, kernel_name, &status);
	checkErr(status, "Failed building kernel");

	return ker;
}

void blackAndWhite(int* r, int* g, int* b, int** ret,
		 size_t nb_pixel, size_t data_size){

	// Create buffers 
	cl_mem red = 	createRBuffer(context, data_size, r);
	cl_mem green = createRBuffer(context, data_size, g);
	cl_mem blue = 	createRBuffer(context, data_size, b);
	cl_mem grey =	createWBuffer(context, data_size, NULL);

	// Free rgb buffers
	free(r);
	free(g);
	free(b);

	// create kernel
	cl_kernel greyshades = createKernel(program, "grey_shade");

	//loading kernel arguments
	printf("Loading kernel args\n");
	printf("Red, ");
	status = clSetKernelArg(greyshades, 0, sizeof(cl_mem), &red);
	checkErr(status, "Failed loading kernel args");
	printf("green, ");
	status = clSetKernelArg(greyshades, 1, sizeof(cl_mem), &green);
	checkErr(status, "Failed loading kernel args");
	printf("blue, ");
	status = clSetKernelArg(greyshades, 2, sizeof(cl_mem), &blue);
	checkErr(status, "Failed loading kernel args");
	printf("grey, ");
	status = clSetKernelArg(greyshades, 3, sizeof(cl_mem), &grey);
	checkErr(status, "Failed loading kernel args");

	size_t globalWorkSize[1];	
	globalWorkSize[0] = nb_pixel;

	int *img = (int*)malloc(data_size);
	
	// Executing kernel
	printf("Executing kernel : ");
	status = clEnqueueNDRangeKernel(
		queue, greyshades, 1, NULL, globalWorkSize, NULL, 0, NULL,NULL);
	checkErr(status, "Failed executing kernel");

	// Reading results
	printf("Reading results : ");
	status = clEnqueueReadBuffer(
			queue, grey, CL_TRUE, 0, data_size, img, 0, NULL, NULL);
	checkErr(status, "Failed reading result from buffer");

	*ret = img;

	// Cleanup
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
	if(greyshades){
		clReleaseKernel(greyshades);
		greyshades = NULL;
	}
}

void edgeD(	int* gShades , int** sobel, int width, int height,
	 	size_t nb_pixel, size_t data_size){

	// ret value
	int* edgeImg = (int*) malloc(data_size);
	
	// create buffers
	cl_mem grey = createRBuffer(context, data_size, gShades);	
	cl_mem edges = createWBuffer(context, data_size, NULL);

	// create kernel
	cl_kernel edgeDetection = createKernel(program, "sobel");
	size_t globalWorkSize[1];	
	globalWorkSize[0] = nb_pixel;


	// Load kernel args	
	printf("Loading kernel args :\n");
	printf("Grey shades, ");
        status = clSetKernelArg(edgeDetection, 0, sizeof(cl_mem), &grey);
        checkErr(status, "Failed loading kernel args");
	
	printf("width, ");
	status = clSetKernelArg(edgeDetection, 1, sizeof(int), &width);
	checkErr(status, "Failed loading kernel args");

	printf("total pixels, ");
	status = clSetKernelArg(edgeDetection, 2, sizeof(int), &nb_pixel);
	checkErr(status, "Failed loading kernel args");

	printf("Sobel buffer, ");
	status = clSetKernelArg(edgeDetection, 3, sizeof(cl_mem), &edges);
	checkErr(status, "Failed loading kernel args");

	// Executing kernel
	printf("Executing kernel : ");
	status = clEnqueueNDRangeKernel(
		queue, edgeDetection, 1,NULL, globalWorkSize, NULL, 0, NULL,NULL);
	checkErr(status, "Failed executing kernel");

	printf("Reading results : ");
	status = clEnqueueReadBuffer(
			queue, edges, CL_TRUE, 0, data_size, edgeImg, 0, NULL, NULL);
	checkErr(status, "Failed reading result from buffer");

	*sobel = edgeImg;

	// cleanup
	if(grey){
		clReleaseMemObject(grey);
		grey = NULL;
	}
	if(edges){
		clReleaseMemObject(edges);
		edges = NULL;
	}
	if(edgeDetection){
		clReleaseKernel(edgeDetection);
		edgeDetection = NULL;
	}
}

void houghLine(	int* sobel, int** houghL, int width, int height,
		size_t nb_pixel, size_t data_size){

	float discStepPhi = 0.012;
	float discStepR =1.25;

	// dimension of accumaltor
	int phiDim = (int) (M_PI/ discStepPhi);
	int rDim = (int) (((width + height) * 2 + 1) / discStepR);

	int* acc = (int*) malloc(phiDim * rDim * sizeof(int));

	// pre compute cos and sin
	float *tabSin, *tabCos;

	tabSin = (float*) malloc(phiDim * sizeof(float));
	tabCos = (float*) malloc(phiDim * sizeof(float));

	float invR = 1.0/discStepR;

	for(int phi = 0 ; phi < phiDim ; phi++){
		float phiFloat = phi * discStepPhi;

		tabSin[phi] = (float)(sin(phiFloat));
		tabCos[phi] = (float)(cos(phiFloat));
	}		

	// create buffers
	cl_mem sinBuf = createRBuffer(context, phiDim * sizeof(float), tabSin);
	cl_mem cosBuf = createRBuffer(context, phiDim * sizeof(float), tabCos);
	cl_mem edges = createRBuffer(context, data_size, sobel);
	cl_mem lines = createWBuffer(context, phiDim * rDim * sizeof(int),NULL);  

	// create kernel
	cl_kernel houghLineKer =  createKernel(program, "houghLine");
	size_t globalWorkSize[1];	
	globalWorkSize[0] = nb_pixel;

	//load kernel args
	printf("Loading kernel args :\n");
	
	// cleanup
	free(tabSin);
	free(tabCos);	
	
	if(sinBuf){
		clReleaseMemObject(sinBuf);
		sinBuf = NULL;
	}
	if(cosBuf){
		clReleaseMemObject(cosBuf);
		cosBuf = NULL;
	}
	if(edges){
		clReleaseMemObject(edges);
		edges = NULL;
	}
	if(lines){
		clReleaseMemObject(lines);
		lines = NULL;
	}
	if(houghLineKer){
		clReleaseKernel(houghLineKer);
		houghLineKer = NULL;
	}

}

void cleanup(){
	// here free all program , queue , context 	
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

