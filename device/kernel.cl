__kernel void vec_double(__global int * A){
	int id = get_global_id(0);
	A[id] = 2 * A[id];
}

