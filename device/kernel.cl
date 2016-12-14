__kernel void grey_shade(__global const int restrict *R,
			 __global const int restrict *G,
			 __global const int restrict *B,
			 __global int* restrict grey){
	int id = get_global_id(0);
	grey[id] = (R[id] + G[id] + B[id])/3;

}

__kernel void sobel(	__global const int restrict *img,
		 	int w, 
			int totPx,
			__global int restrict *sobel){

	int id = get_global_id(0);

	int gradX = 0;
	int gradY = 0;
	
	if( 	id < w ||
		id > (totPx - w) || 
		id % w == 0 || 
		id % w == (w - 1) ){
	}else{
		gradX = - img[id - w -1] - 2 * img[id -1] - img[id + w -1]
			+ img[id - w +1] + 2 * img[id +1] + img[id + w +1];

		gradY = - img[id - w -1] - 2 * img[id -w] - img[id - w +1]
			+ img[id + w -1] + 2 * img[id +w] + img[id + w +1];

		sobel[id] = sqrt(gradX * gradX + gradY * gradY);
	}
}
