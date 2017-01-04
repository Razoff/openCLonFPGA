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

#define NB_LINES 40 
#define DISCRETE_PHI 0.012
#define DISCRETE_R 1.25

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
void findLine(int* accumulator, size_t nbLine, size_t accSize, int** ids);

void checkErr(cl_int status, const char *errmsg);

// openCL variables
cl_int status; // for error code
cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_context context = NULL;
cl_command_queue queue = NULL;
cl_program program = NULL;

int accumulator_s;
int rDim_s;
int phiDim_s;

int main(){
	// begin main
	printf("YOLO world\n");

	// image paremeters
	int width;
	int height;
	int *r,*g,*b,*img,*sobel,*accumulator,*lineIDs;
	png_bytep *row_pointers;

	// Kernel var
	if(openImg(&width, &height, &row_pointers) != 0){
		printf("Failed opening image\n");
		exit(1);
	}

	size_t nb_pixel = width * height;
	size_t data_size = nb_pixel * sizeof(int);

	if(getRGBpixel(&r,&g,&b,width,height, row_pointers) != 0){
		printf("Failed getting RGB composants\n");
		exit(1);
	}

	init();
	
	// apply kernel to output black and white png
	blackAndWhite(r, g, b, &img, nb_pixel, data_size);

	// edge detection
	edgeD(img, &sobel, width, height, nb_pixel, data_size);	

	// line detection accumulator : r,phi accumulator : (r,phi)
	houghLine(sobel,&accumulator, width, height, nb_pixel, data_size);

	// find NB_LINES best lines
	findLine(accumulator, NB_LINES, accumulator_s, &lineIDs );

	process(width, height, row_pointers, sobel);

	printf("Draw lines \n");	
	for(int i = 0 ; i < NB_LINES ; i++){
		draw_line(row_pointers, rDim_s, phiDim_s, lineIDs[i],
		 DISCRETE_R, DISCRETE_PHI, width, height);
	}
	write_png_file(width, height, row_pointers);

	printf("Cleaning up data (avoid memory leaks)\n");	
	cleanup();

	free(lineIDs);
	free(accumulator);
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
	
	printf("Number of openCL plateform : %d\n", num_platforms);

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

	char deviceName[1024]; // gonna hold device name
	char vendorName[1024]; // gonna hold vendor name

	clGetDeviceInfo(dIDs[0], CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
	clGetDeviceInfo(dIDs[0], CL_DEVICE_VENDOR, sizeof(vendorName), vendorName, NULL);

	printf("Executing openCL kernel on %s\n", deviceName);
	printf("Sold by : %s\n", vendorName);

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

	/* //CODE FOR ANYTHING THAT IS NOT FPGA
	FILE* fp;
	const char fileName[] = "./device/kernel.cl";
	size_t source_size;
	char* source_str;
	cl_int ret;

	fp = fopen(fileName, "r");

	source_str = (char*) malloc(0x100000);
	source_size = fread(source_str,1, 0x100000,fp);
	fclose(fp);

	cl_program prog = clCreateProgramWithSource(ctx, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);
	checkErr(ret, "Failed create program");

	ret = clBuildProgram(prog, 1, &dID, NULL,NULL,NULL);
	checkErr(ret, "Failed build program");

	return prog;
*/
	cl_program prog;
	FILE* fp;
	size_t size;
	unsigned char* binary =  NULL;
	cl_int binaryStatus;
	
	printf("Looking for .aocx file\n");

	// Open file and get size
	fp = fopen("bin/kernel.aocx","r");

	if(fp == NULL){	
		printf("Cannot find AOCX file \n");
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	// read .aocx
	binary = (unsigned char*)malloc(size + 1);  
	binary[size] = '\0'; // EOF char
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

	float discStepPhi = DISCRETE_PHI;
	float discStepR = DISCRETE_R;

	// dimension of accumaltor
	int phiDim = (int) (M_PI/ discStepPhi);
	int rDim = (int) (((width + height) * 2 + 1) / discStepR);

	rDim_s = rDim;
	phiDim_s = phiDim;

	printf("Accumulator size :  %d\n",rDim_s * phiDim);

	int* acc = (int*) malloc(phiDim * rDim * sizeof(int));
	
	accumulator_s = phiDim * rDim;

	// pre compute cos and sin
	float *tabSin, *tabCos;

	tabSin = (float*) malloc(phiDim * sizeof(float));
	tabCos = (float*) malloc(phiDim * sizeof(float));

	if(tabSin == NULL || tabCos == NULL){
		printf("Failed memory allocation\n");
		exit(1);
	}

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
	printf("Edge image, ");	
	status = clSetKernelArg(houghLineKer, 0, sizeof(cl_mem), &edges);
	checkErr(status, "Failed loading kernel args");

	printf("Cosinus table, ");
	status = clSetKernelArg(houghLineKer, 1, sizeof(cl_mem), &cosBuf);
	checkErr(status, "Failed loading kernel args");

	printf("Sinus table, ");
	status = clSetKernelArg(houghLineKer, 2, sizeof(cl_mem), &sinBuf);
	checkErr(status, "Failed loading kernel args");

	printf("Width, ");
	status = clSetKernelArg(houghLineKer, 3, sizeof(int), &width);
	checkErr(status, "Failed loading kernel args");

	printf("rDim, ");
	status = clSetKernelArg(houghLineKer, 4, sizeof(int), &rDim);
	checkErr(status, "Failed loading kernel args");

	printf("Dicrete step phi, ");
	status = clSetKernelArg(houghLineKer, 5, sizeof(int), &phiDim);
	checkErr(status, "Failed loading kernel args");

	printf("Discrete step r, ");
	status = clSetKernelArg(houghLineKer, 6, sizeof(float), &discStepR);
	checkErr(status, "Failed loading kernel args");

	printf("Accumulator, ");
	status = clSetKernelArg(houghLineKer, 7, sizeof(cl_mem), &lines);
	checkErr(status, "Failed loading kernel args");

	// Executing kernel
	printf("Executing kernel : ");
	status = clEnqueueNDRangeKernel(
		queue, houghLineKer,1, NULL,globalWorkSize,NULL, 0, NULL, NULL);
	checkErr(status, "Failed executing kernel");

	// Read result back
	printf("Reading results : ");
	status = clEnqueueReadBuffer(
	queue, lines, CL_TRUE, 0, phiDim * rDim * sizeof(int), acc,0,NULL,NULL
	);
	checkErr(status, "Failed reading results");
	
	// Assign acc for return value
	*houghL = acc;
	
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
void findLine(int* accumulator, size_t nbLine, size_t accSize, int** ids){
	
	int *id , *score;
	
	printf("Find the %d most important lines\n", nbLine);

	id = (int*) malloc(nbLine * sizeof(int));
	score = (int*) malloc(nbLine * sizeof(int));

	if(id == NULL || score == NULL){
		printf("Failed memory allocation\n");
		exit(1);
	}

	for(int i = 0 ; i < nbLine ; i++){ // use calloc instead ?
		score[i] = 0;
		id[i] = 0;
	}

	for(int i = 0; i < accSize; i++){
		if(accumulator[i] > score[0]){ 
		// if vote greater than smaller value add it
			score[0] = accumulator[i];
			id[0] = i;

			for(int j = 0; j < (nbLine-1); j++){
				if(score[j] > score[j + 1]){
					// swap value if it is greater than before
					int tmpS = score[j];
					int tmpI = id[j];
					score[j] = score[j+1];
					score[j+1] = tmpS;
					id[j] = id[j+1];
					id[j+1] = tmpI;
				}				
			}
		}			
	}	

	// Return id free vote count
	free(score);
	*ids = id;
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

void checkErr(cl_int status, const char *errmsg){
	if(status != CL_SUCCESS){
		printf("\nOperation failed : %s\n", errmsg);
		exit(status);
	}else{
		printf("SUCCESS\n");
	}
}

