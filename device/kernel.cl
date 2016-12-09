__kernel void grey_shade(__global int *R,
			 __global int *G,
			 __global int *B,
			 __global int *grey){
	int id = get_global_id(0);
	grey[id] = (R[id] + G[id] + B[id])/3;

}
