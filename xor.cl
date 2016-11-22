uint get_row_nr_4(uint xi0, uint round){
uint row;
#if NR_ROWS_LOG == 16
    if (!(round % 2))
        row = (xi0 & 0xffff);
    else
        // if we have in hex: "ab cd ef..." (little endian xi0) then this
        // formula computes the row as 0xdebc. it skips the 'a' nibble as it
        // is part of the PREFIX. The Xi will be stored starting with "ef...";
        // 'e' will be considered padding and 'f' is part of the current PREFIX
        row = ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
            ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 18
    if (!(round % 2))
        row = (xi0 & 0xffff) | ((xi0 & 0xc00000) >> 6);
    else
        row = ((xi0 & 0xc0000) >> 2) |
            ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
            ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 19
    if (!(round % 2))
        row = (xi0 & 0xffff) | ((xi0 & 0xe00000) >> 5);
    else
        row = ((xi0 & 0xe0000) >> 1) |
            ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
            ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 20
    if (!(round % 2))
        row = (xi0 & 0xffff) | ((xi0 & 0xf00000) >> 4);
    else
        row = ((xi0 & 0xf0000) >> 0) |
            ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
            ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#else
#error "unsupported NR_ROWS_LOG"
#endif
        return row;
}

uint get_row_nr_8(ulong xi0, uint round){
uint row;
#if NR_ROWS_LOG == 16
    if (!(round % 2))
	row = (xi0 & 0xffff);
    else
	// if we have in hex: "ab cd ef..." (little endian xi0) then this
	// formula computes the row as 0xdebc. it skips the 'a' nibble as it
	// is part of the PREFIX. The Xi will be stored starting with "ef...";
	// 'e' will be considered padding and 'f' is part of the current PREFIX
	row = ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 18
    if (!(round % 2))
	row = (xi0 & 0xffff) | ((xi0 & 0xc00000) >> 6);
    else
	row = ((xi0 & 0xc0000) >> 2) |
	    ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 19
    if (!(round % 2))
	row = (xi0 & 0xffff) | ((xi0 & 0xe00000) >> 5);
    else
	row = ((xi0 & 0xe0000) >> 1) |
	    ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#elif NR_ROWS_LOG == 20
    if (!(round % 2))
	row = (xi0 & 0xffff) | ((xi0 & 0xf00000) >> 4);
    else
	row = ((xi0 & 0xf0000) >> 0) |
	    ((xi0 & 0xf00) << 4) | ((xi0 & 0xf00000) >> 12) |
	    ((xi0 & 0xf) << 4) | ((xi0 & 0xf000) >> 12);
#else
#error "unsupported NR_ROWS_LOG"
#endif
	return row;
}

void store8(__global char *p,ulong store){
	asm volatile ( "st.global.cs.b64  [%0], %1;\n\t" :: "l"(p), "l" (store));
}

void store4(__global char *p,uint store){
        asm volatile ( "st.global.cs.b32  [%0], %1;\n\t" :: "l"(p), "r" (store));
}

void store_ulong2(__global char *p,ulong2 store){
	asm volatile ( "st.global.cs.v2.b64  [%0],{ %1, %2 };\n\t" :: "l"(p), "l" (store.x), "l" (store.y));
}

void store_uint2(__global char *p,uint2 store){
        asm volatile ( "st.global.cs.v2.b32  [%0],{ %1, %2 };\n\t" :: "l"(p), "r" (store.x), "r" (store.y));
}

void store_uint4(__global char *p,uint4 store){
        asm volatile ( "st.global.cs.v4.b32  [%0],{ %1, %2, %3, %4 };\n\t" :: "l"(p), "r" (store.x), "r" (store.y), "r" (store.z), "r" (store.w));
}

ulong load8_last(__global ulong *p,uint offset){
	p=(__global ulong *)((__global char *)p + offset); 
        ulong r;
        asm volatile ( "ld.global.cs.nc.b64  %0, [%1];\n\t" : "=l"(r) : "l"(p));
        return r;
}

ulong load8(__global ulong *p,uint offset){
	p=(__global ulong *)((__global char *)p + offset); 
        ulong r;
        asm volatile ( "ld.global.cs.nc.b64  %0, [%1];\n\t" : "=l"(r) : "l"(p));
        return r;
}



ulong2 load16l(__global ulong *p,uint offset){
        p=(__global ulong *)((__global char *)p + offset); 
        ulong2 r;
        asm volatile ( "ld.global.cs.nc.v2.b64  {%0,%1}, [%2];\n\t" : "=l"(r.x), "=l"(r.y) : "l"(p));
        return r;
}


uint load4_last(__global ulong *p,uint offset){
	p=(__global ulong *)((__global char *)p + offset); 
        uint r;
        asm volatile ( "ld.global.cs.nc.b32  %0, [%1];\n\t" : "=r"(r) : "l"(p));
        return r;
}

uint load4(__global ulong *p,int offset){
	p=(__global ulong *)((__global char *)p + offset); 
        uint r;
        asm volatile ( "ld.global.cs.nc.b32  %0, [%1];\n\t" : "=r"(r) : "l"(p));
        return r;
}

void trigger_err(){
	load8_last((__global ulong *)-1,0);
}

#define nv64to16(a,b,c,d,X) asm volatile( "mov.b64 {%0,%1,%2,%3}, %4; \n\t" : "=r"(a), "=r"(b), "=r"(c), "=r"(d) : "r"(X))


// Round 1

uint xor_and_store1(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD

	ulong2 loada,loadb;
	xi0 = load8(a++,0) ^ load8(b++,0);
//	loada = *(__global ulong2 *)a;
	loada = load16l(a,0);
	loadb = load16l(b,0);
	xi1 = loada.x ^ loadb.x;
	xi2 = loada.y ^ loadb.y;


/*
	xi0 = *(a++) ^ *(b++);
	xi1 = *(a++) ^ *(b++);
	xi2 = *a ^ *b;
	xi3 = 0;
*/
//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

	//256bit shift


	asm("{ .reg .b16 a0,a1,a2,a3,b0,b1,b2,b3,c0,c1,c2,c3; \n\t"
	"mov.b64 {a0,a1,a2,a3}, %4;\n\t"
	"mov.b64 {b0,b1,b2,b3}, %5;\n\t"
	"mov.b64 {c0,c1,c2,c3}, %6;\n\t"

	"mov.b64 %0, {a1,a2,a3,b0};\n\t"
	"mov.b64 %1, {b1,b2,b3,c0};\n\t"
	"mov.b64 %2, {c1,c2,c3,0};\n\t"
	"mov.b32 %3, {a0,a1};\n\t"
	"}\n" : "=l"(xi0), "=l"(xi1), "=l" (xi2), "=r"(_row): "l"(xi0), "l"(xi1), "l"(xi2));


//      row = get_row_nr_4((uint)xi0,round);	
	row = get_row_nr_4(_row,round);

//        xi0 = (xi0 >> 16) | (xi1 << (64 - 16));
//        xi1 = (xi1 >> 16) | (xi2 << (64 - 16));
//        xi2 = (xi2 >> 16);
	
//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE
//        *(__global uint *)(p - 4) = i;
//        *(__global ulong *)(p + 0) = xi0;
//	*(__global ulong *)(p + 8) = xi1;
//	*(__global ulong *)(p + 16) = xi2;


	ulong2 store0;
	ulong2 store1;
	nv32to64(store0.x,0,i);
	store0.y=xi0;
//	*(__global ulong2 *)(pp)=store0;
	store_ulong2(pp,store0);
	store1.x=xi1;
	store1.y=xi2;
//	*(__global ulong2 *)(pp+16)=store1;
	store_ulong2(pp+16,store1);
return 0;
}



// Round 2

uint xor_and_store2(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;

	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD
	ulong2 loada,loadb;
	xi0 = load8(a++,0) ^ load8(b++,0);
	loada = load16l(a,0);
	loadb = load16l(b,0);
	xi1 = loada.x ^ loadb.x;
	xi2 = loada.y ^ loadb.y;


/*
	xi0 = *(a++) ^ *(b++);
	xi1 = *(a++) ^ *(b++);
	xi2 = *a ^ *b;
	xi3 = 0;
*/
//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

	//256bit shift



//7 op asm32 4 op + 3 op devectorize

	uint _xi0l,_xi0h,_xi1l,_xi1h,_xi2l,_xi2h;
	asm("{\n\t"
			".reg .b32 a0,a1,b0,b1,c0,c1; \n\t"
			"mov.b64 {a0,a1}, %6;\n\t"
        	        "mov.b64 {b0,b1}, %7;\n\t"
                        "mov.b64 {c0,c1}, %8;\n\t"
			
			"shr.b32 %5,a0,8;\n\t"
                        "shf.r.clamp.b32 %0,a0,a1,24; \n\t"
                        "shf.r.clamp.b32 %1,a1,b0,24; \n\t"
                        "shf.r.clamp.b32 %2,b0,b1,24; \n\t"
			"shf.r.clamp.b32 %3,b1,c0,24; \n\t"
			"shf.r.clamp.b32 %4,c0,c1,24; \n\t"

                        "}\n\t"
                        : "=r"(_xi0l), "=r"(_xi0h),"=r"(_xi1l), "=r"(_xi1h), "=r"(_xi2l), "=r"(_row) :  
			"l"(xi0), "l"(xi1), "l"(xi2));

	row = get_row_nr_4(_row,round);

//	xi0 = (xi0 >> 24) | (xi1 << (64 - 24));
//        xi1 = (xi1 >> 24) | (xi2 << (64 - 24));
//        xi2 = (xi2 >> 24);
//

    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
//	*a+=load8_last((__global ulong *)-1);
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE 11 op, asm 9 op, or 6op 32bit

/*
        ulong s0;
	ulong2 store0;

	nv32to64(s0,i,_xi0l);
	nv32to64(store0.x,_xi0h,_xi1l);
	nv32to64(store0.y,_xi1h,_xi2l);
	*(__global ulong *)(p - 4)=s0;
	*(__global ulong2 *)(p + 4)=store0;
*/

	uint2 s0;
	s0.x=i;
	s0.y=_xi0l;
        uint4 store0;
	store0.x=_xi0h;
	store0.y=_xi1l;
	store0.z=_xi1h;
	store0.w=_xi2l;
//	*(__global uint2 *)(p - 4)=s0;
	store_uint2(p-4, s0);
//        *(__global uint4 *)(p + 4)=store0;
	store_uint4(p+4,store0);
/*
	*(__global uint *)(p - 4) = i;
	*(__global uint *)(p + 0) = xi0;
	*(__global ulong *)(p + 4) = (xi0 >> 32) | (xi1 << 32);
	*(__global ulong *)(p + 12) = (xi1 >> 32) | (xi2 << 32);
*/
return 0;
}



//Round3

uint xor_and_store3(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
//	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD
	uint xi0l,xi0h,xi1l,xi1h,xi2l;
	xi0l = load4(a,0) ^ load4(b,0);
	
	if(!xi0l )
		return 0;


	ulong load1,load2;
	load1 = load8(a , 4) ^ load8(b , 4);
	load2 = load8_last(a , 12) ^ load8_last(b , 12);
	nv64to32(xi0h,xi1l,load1);
	nv64to32(xi1h,xi2l,load2);

//     if(!xi0l )
//	*a+=load8_last((__global ulong *)-1);
	// xor 20 bytes
//	xi0 = half_aligned_long(a, 0) ^ half_aligned_long(b, 0);
//	xi1 = half_aligned_long(a, 8) ^ half_aligned_long(b, 8);
//	xi2 = well_aligned_int(a, 16) ^ well_aligned_int(b, 16);
//	ulong2 loada;
//	ulong2 loadb;
	

//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	



	row = get_row_nr_4(xi0l,round);	


	uint _xi0l,_xi0h,_xi1l,_xi1h;
	asm("{\n\t"
                        "shf.r.clamp.b32 %0,%4,%5,16; \n\t"
                        "shf.r.clamp.b32 %1,%5,%6,16; \n\t"
                        "shf.r.clamp.b32 %2,%6,%7,16; \n\t"
                        "shf.r.clamp.b32 %3,%7,%8,16; \n\t"
                        "}\n\t"
                        : "=r"(_xi0l), "=r"(_xi0h),"=r"(_xi1l), "=r"(_xi1h):  
                        "r"(xi0l), "r"(xi0h),"r"(xi1l), "r"(xi1h) , "r"(xi2l));




//        xi0 = (xi0 >> 16) | (xi1 << (64 - 16));
//        xi1 = (xi1 >> 16) | (xi2 << (64 - 16));
//        xi2 = (xi2 >> 16);
	
//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
//	*a+=load8_last((__global ulong *)-1);
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE
	

	

	ulong store0,store1;
       nv32to64(store0,i,_xi0l);
	nv32to64(store1,_xi0h,_xi1l);

//        *(__global ulong *)(p - 4) = store0;
	store8(p - 4,store0);
//        *(__global ulong *)(p + 4) = store1;
	store8(p + 4,store1);
//        *(__global uint *)(p + 12) = _xi1h;
	store4(p + 12,_xi1h);

/*
	 *(__global uint *)(p - 4) = i;
		// store 16 bytes
	*(__global uint *)(p + 0) = xi0;
	*(__global ulong *)(p + 4) = (xi0 >> 32) | (xi1 << 32);
	*(__global uint *)(p + 12) = (xi1 >> 32);
*/
return 0;
}



// Round 4

uint xor_and_store4(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD

//	xi0 = half_aligned_long(a, 0) ^ half_aligned_long(b, 0);
//	xi1 = half_aligned_long(a, 8) ^ half_aligned_long(b, 8);
	

	uint xi0l,xi0h,xi1l,xi1h;
	xi0l = load4(a, 0) ^ load4(b, 0);
	        if(!xi0l )
                return 0;
	xi0h = load4(a, 4) ^ load4(b, 4);
	xi1l = load4(a, 8) ^ load4(b, 8);
	xi1h = load4_last(a, 12) ^ load4_last(b, 12);


//	xi2 = 0;

//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

//256bit shift

	uint _xi0l,_xi0h,_xi1l,_xi1h,_xi2l,_xi2h;
	asm("{\n\t"
                        "shf.r.clamp.b32 %0,%4,%5,24; \n\t"
                        "shf.r.clamp.b32 %1,%5,%6,24; \n\t"
                        "shf.r.clamp.b32 %2,%6,%7,24; \n\t"
			"shr.b32         %3,%7,24; \n\t"
                        "}\n\t"
                        : "=r"(_xi0l), "=r"(_xi0h),"=r"(_xi1l), "=r"(_xi1h):  
			"r"(xi0l), "r"(xi0h), "r"(xi1l), "r"(xi1h));

	row = get_row_nr_4(xi0l >> 8,round);

//            xi0 = (xi0 >> 8) | (xi1 << (64 - 8));
//	    xi1 = (xi1 >> 8);

      //row = get_row_nr_4((uint)xi0,round);	
//	row = get_row_nr_4(_row,round);

 //       xi0 = (xi0 >> 16) | (xi1 << (64 - 16));
 //       xi1 = (xi1 >> 16) | (xi2 << (64 - 16));
 //       xi2 = (xi2 >> 16);
	
//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE

//*(__global uint *)(p - 4) = i;
store4(p-4,i);
//*(__global ulong *)(p + 0) = xi0;
//*(__global ulong *)(p + 8) = xi1;
uint4 store;
	store.x=_xi0l;
	store.y=_xi0h;
	store.z=_xi1l;
	store.w=_xi1h;
//*(__global uint4 *)(p + 0) = store;
store_uint4(p + 0, store);
return 0;
}


// Round 5

uint xor_and_store5(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD

//	xi0 = half_aligned_long(a, 0) ^ half_aligned_long(b, 0);
//	xi1 = half_aligned_long(a, 8) ^ half_aligned_long(b, 8);
	

	uint xi0l,xi0h,xi1l,xi1h;
	xi0l = load4(a, 0) ^ load4(b, 0);
	        if(!xi0l )
                return 0;
	xi0h = load4(a, 4) ^ load4(b, 4);
	xi1l = load4(a, 8) ^ load4(b, 8);
	xi1h = load4_last(a, 12) ^ load4_last(b, 12);


//	xi2 = 0;

//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

//256bit shift

	uint _xi0l,_xi0h,_xi1l,_xi1h,_xi2l,_xi2h;
	asm("{\n\t"
                        "shf.r.clamp.b32 %0,%4,%5,16; \n\t"
                        "shf.r.clamp.b32 %1,%5,%6,16; \n\t"
                        "shf.r.clamp.b32 %2,%6,%7,16; \n\t"
			"shr.b32         %3,%7,16; \n\t"
                        "}\n\t"
                        : "=r"(_xi0l), "=r"(_xi0h),"=r"(_xi1l), "=r"(_xi1h):  
			"r"(xi0l), "r"(xi0h), "r"(xi1l), "r"(xi1h));

	row = get_row_nr_4(xi0l,round);

//            xi0 = (xi0 >> 8) | (xi1 << (64 - 8));
//	    xi1 = (xi1 >> 8);

      //row = get_row_nr_4((uint)xi0,round);	
//	row = get_row_nr_4(_row,round);

 //       xi0 = (xi0 >> 16) | (xi1 << (64 - 16));
 //       xi1 = (xi1 >> 16) | (xi2 << (64 - 16));
 //       xi2 = (xi2 >> 16);
	
//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE

//*(__global uint *)(p - 4) = i;
store4(p-4,i);
//*(__global ulong *)(p + 0) = xi0;
//*(__global ulong *)(p + 8) = xi1;
uint4 store;
	store.x=_xi0l;
	store.y=_xi0h;
	store.z=_xi1l;
	store.w=_xi1h;
//*(__global uint4 *)(p + 0) = store;
store_uint4(p + 0, store);
return 0;
}




// Round 6

uint xor_and_store6(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD
	uint xi0l,xi0h,xi1l;

	xi0 = load8(a++,0) ^ load8(b++,0);

	if(!xi0 )
                return 0;
	xi1l = load4_last(a,0) ^ load4_last(b,0);
	
	nv64to32(xi0l,xi0h,xi0);

//	xi0 = (xi0 >> 8) | (xi1 << (64 - 8));
//	xi1 = (xi1 >> 8);

//	xi2 = 0;

//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

//256bit shift

	uint _xi0l,_xi0h,_xi1l,_xi1h,_xi2l,_xi2h;
	asm("{\n\t"
                        "shf.r.clamp.b32 %0,%3,%4,24; \n\t"
                        "shf.r.clamp.b32 %1,%4,%5,24; \n\t"
			"shr.b32         %2,%5,24; \n\t"
                        "}\n\t"
                        : "=r"(_xi0l), "=r"(_xi0h),"=r"(_xi1l):  
			"r"(xi0l), "r"(xi0h), "r"(xi1l));

	row = get_row_nr_4(xi0l >> 8,round);

	
//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE
	
//	*(__global uint *)(p - 4) = i;
	ulong store;
	nv32to64(store,i,_xi0l);
	store8(p - 4,store);
	// *(__global ulong *)(p - 4)= store;
//	*(__global uint *)(p + 0) = _xi0l;
//	*(__global uint *)(p + 4) = _xi0h;
	store4(p+4,_xi0h);
return 0;
}


// Round 7

uint xor_and_store7(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD

	uint xi0l,xi0h;
	xi0l = load4(a, 0) ^ load4(b, 0);
	        if(!xi0l )
                return 0;
	xi0h = load4_last(a, 4) ^ load4_last(b, 4);
//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

//256bit shift


	row = get_row_nr_4(xi0l,round);

	uint _xi0l,_xi0h;
	asm("{\n\t"
                        "shf.r.clamp.b32 %0,%2,%3,16; \n\t"
			"shr.b32         %1,%3,16; \n\t"
                        "}\n\t"
                        : "=r"(_xi0l), "=r"(_xi0h):  
			"r"(xi0l), "r"(xi0h));
//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE
	
	uint2 store;
	store.x=i;
	store.y=_xi0l;
//	*(__global uint2 *)(p - 4) = store;
	store_uint2(p-4,store);
//	*(__global uint *)(p + 0) = _xi0l;
//	*(__global uint *)(p + 4) = _xi0h;
	store4(p + 4 , _xi0h);
return 0;
}

// Round 8

uint xor_and_store8(uint round, __global char *ht_dst, uint x_row,
        uint slot_a, uint slot_b, __global ulong *a, __global ulong *b,
        __global uint *rowCounters){
	
	ulong xi0, xi1, xi2,xi3;
	uint _row;
	uint row;
	__global char       *p;
        uint                cnt;
//LOAD

	uint xi0l,xi0h;
	xi0l = load4(a, 0) ^ load4(b, 0);
	        if(!xi0l )
                return 0;
	xi0h = load4_last(a, 4) ^ load4_last(b, 4);
//
	uint i = ENCODE_INPUTS(x_row, slot_a, slot_b);
	

//256bit shift


	row = get_row_nr_4(xi0l >> 8,round);

	
	uint _xi0l,_xi0h,_xi1l,_xi1h,_xi2l,_xi2h;
	asm("{\n\t"
                        "shf.r.clamp.b32 %0,%1,%2,24; \n\t"
                        "}\n\t"
                        : "=r"(_xi0l):  
			"r"(xi0l), "r"(xi0h));

//
	
    p = ht_dst + row * NR_SLOTS * SLOT_LEN;
    uint rowIdx = row/ROWS_PER_UINT;
    uint rowOffset = BITS_PER_ROW*(row%ROWS_PER_UINT);
    uint xcnt = atomic_add(rowCounters + rowIdx, 1 << rowOffset);
    xcnt = (xcnt >> rowOffset) & ROW_MASK;
    cnt = xcnt;
    if (cnt >= NR_SLOTS)
      {
	// avoid overflows
	atomic_sub(rowCounters + rowIdx, 1 << rowOffset);
	return 1;
      }
    __global char       *pp = p + cnt * SLOT_LEN;
    p = pp + xi_offset_for_round(round);
//

//STORE
	
//	uint2 store;
//	store.x=i;
//	store.y=_xi0l;

//	*(__global uint *)(p - 4) = i;
//	*(__global uint *)(p + 0) = _xi0l;
	store4(p-4, i);
	store4(p+0, _xi0l);
return 0;
}


