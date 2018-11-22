// #ifdef __cplusplus
// extern "C" {
// #endif

//========================================================================================================================================================================================================200
//	KERNEL_CPU HEADER
//========================================================================================================================================================================================================200

void 
kernel_cpu(	int cores_arg,

			record *records,
			knode *knodes,
			long knodes_elem,
			long records_elem,

			int order,
			long maxheight,
			int count,

			long *currKnode,
			long *offset,
			int *keys,
			record *ans);

kernel_gpu(	int cores_arg,

			record *records,
			knode *knodes,
			long knodes_elem,
			long records_elem,

			int order,
			long maxheight,
			int count,

			long *currKnode,
			long *offset,
			int *keys,
			record *ans);

//========================================================================================================================================================================================================200
//	END
//========================================================================================================================================================================================================200

// #ifdef __cplusplus
// }
// #endif