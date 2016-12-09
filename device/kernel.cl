__kernel void grey_shade(__global const int *R,
			 __global const int *G,
			 __global const int *B,
			 __global int* restrict grey){
	int id = get_global_id(0);
	grey[id] = (R[id] + G[id] + B[id])/3;

}
