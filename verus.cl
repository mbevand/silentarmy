

#define BYTE(x, y)			(amd_bfe((uint)((x) >> ((y >= 32U) ? 32U : 0U)), (y) - (((y) >= 32) ? 32U : 0), 8U))

__constant static const uint sbox[64] = { 0x7b777c63, 0xc56f6bf2, 0x2b670130, 0x76abd7fe, 0x7dc982ca, 0xf04759fa, 0xafa2d4ad, 0xc072a49c, 0x2693fdb7, 0xccf73f36, 0xf1e5a534, 0x1531d871, 0xc323c704, 0x9a059618, 0xe2801207, 0x75b227eb, 0x1a2c8309, 0xa05a6e1b, 0xb3d63b52, 0x842fe329, 0xed00d153, 0x5bb1fc20, 0x39becb6a, 0xcf584c4a, 0xfbaaefd0, 0x85334d43, 0x7f02f945, 0xa89f3c50, 0x8f40a351, 0xf5389d92, 0x21dab6bc, 0xd2f3ff10, 0xec130ccd, 0x1744975f, 0x3d7ea7c4, 0x73195d64, 0xdc4f8160, 0x88902a22, 0x14b8ee46, 0xdb0b5ede, 0x0a3a32e0, 0x5c240649, 0x62acd3c2, 0x79e49591, 0x6d37c8e7, 0xa94ed58d, 0xeaf4566c, 0x08ae7a65, 0x2e2578ba, 0xc6b4a61c, 0x1f74dde8, 0x8a8bbd4b, 0x66b53e70, 0x0ef60348, 0xb9573561, 0x9e1dc186, 0x1198f8e1, 0x948ed969, 0xe9871e9b, 0xdf2855ce, 0x0d89a18c, 0x6842e6bf, 0x0f2d9941, 0x16bb54b0 };

#define XT(x) (((x) << 1) ^ ((((x) >> 7) & 1) * 0x1b))

void memcpy_decker(unsigned char *dst, unsigned char *src, int len) {
	int i;
	for (i = 0; i< len; i++) { dst[i] = src[i]; }
}
void aesenc(unsigned char *s, __local uint sharedMemory1[64])
{
	uchar i, t, u;
	uchar v[4][4];

	for (i = 0; i < 16; ++i) {
		v[((i >> 2) + 4 - (i & 3)) & 3][i & 3] = ((uchar*)&sharedMemory1)[s[i]];
	}

	for (i = 0; i < 4; ++i) {
		t = v[i][0];
		u = v[i][0] ^ v[i][1] ^ v[i][2] ^ v[i][3];
		v[i][0] = v[i][0] ^ u ^ XT(v[i][0] ^ v[i][1]);
		v[i][1] = v[i][1] ^ u ^ XT(v[i][1] ^ v[i][2]);
		v[i][2] = v[i][2] ^ u ^ XT(v[i][2] ^ v[i][3]);
		v[i][3] = v[i][3] ^ u ^ XT(v[i][3] ^ t);
	}
	for (i = 0; i < 16; ++i) {
		s[i] = (unsigned char)v[i >> 2][i & 3]; // VerusHash have 0 rc vector
	}
}

// Simulate _mm_unpacklo_epi32
void unpacklo32(unsigned char *t, unsigned char *a, unsigned char *b)
{
	unsigned char tmp[16];
	memcpy_decker(tmp, a, 4);
	memcpy_decker(tmp + 4, b, 4);
	memcpy_decker(tmp + 8, a + 4, 4);
	memcpy_decker(tmp + 12, b + 4, 4);
	memcpy_decker(t, tmp, 16);
}

// Simulate _mm_unpackhi_epi32
void unpackhi32(unsigned char *t, unsigned char *a, unsigned char *b)
{
	unsigned char tmp[16];
	memcpy_decker(tmp, a + 8, 4);
	memcpy_decker(tmp + 4, b + 8, 4);
	memcpy_decker(tmp + 8, a + 12, 4);
	memcpy_decker(tmp + 12, b + 12, 4);
	memcpy_decker(t, tmp, 16);

}
__kernel __attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void kernel_verushash(__global uchar *midstate, __global uint *output, __global ulong *target)
{ 
	
	__private uint gid = get_global_id(0);
	__private uchar s[64] , tmp[16];
	__local uint sharedMemory1[64];
	__private uchar n[64];
        
    
	int i, j;

	if (get_local_id(0) == 1)
		for (i = 0; i < 64; ++i)
			sharedMemory1[i] = sbox[i];

	for (i = 0; i < 64; ++i)
		s[i] = (idstate[i];  //copy midstate to s

	for (i = 32; i < 64; ++i)
			s[i] = 0x00;  //set send ulong to 000
	((uint *)&s)[8] = gid;
	
	mem_fence(CLK_LOCAL_MEM_FENCE);

	for (i = 0; i < 5; ++i) {
		// aes round(s)
		
		for (j = 0; j < 2; ++j) {

			aesenc(s, sharedMemory1);
			aesenc(s + 16, sharedMemory1);
			aesenc(s + 32, sharedMemory1);
			aesenc(s + 48, sharedMemory1);
		}

		unpacklo32(tmp, s, s + 16);

		unpackhi32(s, s, s + 16);
		unpacklo32(s + 16, s + 32, s + 48);

		unpackhi32(s + 32, s + 32, s + 48);
		unpacklo32(s + 48, s, s + 32);
		unpackhi32(s, s, s + 32);
		unpackhi32(s + 32, s + 16, tmp);

		unpacklo32(s + 16, s + 16, tmp);

	}

	//memcpy_decker(out + 32, s + 32, 32);

	for (i = 48; i < 56; i++) {
		s[i] = s[i] ^ midstate[i];
	}

	/* Truncated */
	//memcpy_decker(out, buf + 8, 8);
	//memcpy_decker(out + 8, buf + 24, 8);
	//memcpy_decker(out + 16, s + 32, 8);
	//memcpy_decker(s + 24, s + 48, 8);
	//memcpy_decker(n, s, 32);  ///TODOuse a ulong8 all the way through 

	if(((ulong*)&s)[3] <= target) output[0] = gid;
}
// Simulate _mm_aesenc_si128 instructions from AESNI


