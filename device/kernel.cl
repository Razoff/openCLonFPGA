__kernel void grey_shade(__global const int* restrict R,
			 __global const int* restrict G,
			 __global const int* restrict B,
			 __global int* restrict grey){
	int id = get_global_id(0);
	grey[id] = (R[id] + G[id] + B[id])/3;

}

__kernel void sobel(	__global const int* restrict img,
		 	int w, 
			int totPx,
			__global int* restrict sobel){

	int id = get_global_id(0);

	int gradX = 0;
	int gradY = 0;
	int grad  = 0;
	
	// we dont want to evaluate anything on the sides
	if( 	id < w ||
		id > (totPx - w) || 
		id % w == 0 || 
		id % w == (w - 1) ){
	}else{
		gradX = - img[id - w -1] - 2 * img[id -1] - img[id + w -1]
			+ img[id - w +1] + 2 * img[id +1] + img[id + w +1];

		gradY = - img[id - w -1] - 2 * img[id -w] - img[id - w +1]
			+ img[id + w -1] + 2 * img[id +w] + img[id + w +1];

		//grad = sqrt(gradX * gradX + gradY * gradY); // FPGA code
          grad = gradX + gradY; // intel CPU code

		if(grad < 150){
			sobel[id] = 0;
		}else if (grad > 255){
			sobel[id] = 255;
		}else{
			sobel[id] = grad;
		}
	}
}

__kernel void houghLine(	__global const int* restrict img,
				__global const float* restrict cosinus,
				__global const float* restrict sinus,
				int width,
				int rDim,
				int phiDim,
				float discStepR,
				__global int* acc){
	
	int id = get_global_id(0);

	// pos in x = k * width + posX in current line
	// pos in y = id / width 
	int x = id % width;
	int y = id / width; // always floored result when div two positive int

	// if the pixel is not 0 we are on an edge
	if(img[id] != 0){
		for(int phi = 0; phi < phiDim; phi ++){
			float rFloat = x * cosinus[phi] + y * sinus[phi];
			int r = (int) (rFloat / discStepR);
			acc[ rDim * phi + r ] += 1;
		}
	}

} 
