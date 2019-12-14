
#define VERUS_KEY_SIZE 8832
#define VERUS_KEY_SIZE128 552
#define THREADS 64

typedef uint4 uint128m;
typedef ulong uint64_t;
typedef uint uint32_t;
typedef uchar uint8_t;
typedef long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
  
// Simulate C memcpy uint32_t*
void memcpy_uint(unsigned int *dst, unsigned int *src, int len) {
    int i;
    for (i=0;i<len;i++) { dst[i] = src[i]; }
}

// Simulate C memcpy
void memcpy(unsigned char *dst, unsigned char *src, int len) {
    int i;
    for (i=0;i<len;i++) { dst[i] = src[i]; }
}

#define AES2_EMU(s0, s1, rci) \
  aesenc((unsigned char *)&s0, &rc[rci],sharedMemory1); \
  aesenc((unsigned char *)&s1, &rc[rci + 1],sharedMemory1); \
  aesenc((unsigned char *)&s0, &rc[rci + 2],sharedMemory1); \
  aesenc((unsigned char *)&s1, &rc[rci + 3],sharedMemory1);
 
#define AES2_EMU_LOC(s0, s1, rci) \
  aesenc_loc((unsigned char *)&s0, &rc[rci],sharedMemory1); \
  aesenc_loc((unsigned char *)&s1, &rc[rci + 1],sharedMemory1); \
  aesenc_loc((unsigned char *)&s0, &rc[rci + 2],sharedMemory1); \
  aesenc_loc((unsigned char *)&s1, &rc[rci + 3],sharedMemory1); 
  


#define AES4(s0, s1, s2, s3, rci) \
  aesenc((unsigned char *)&s0, &rc[rci],sharedMemory1); \
  aesenc((unsigned char *)&s1, &rc[rci + 1],sharedMemory1); \
  aesenc((unsigned char *)&s2, &rc[rci + 2],sharedMemory1); \
  aesenc((unsigned char *)&s3, &rc[rci + 3],sharedMemory1); \
  aesenc((unsigned char *)&s0, &rc[rci + 4], sharedMemory1); \
  aesenc((unsigned char *)&s1, &rc[rci + 5], sharedMemory1); \
  aesenc((unsigned char *)&s2, &rc[rci + 6], sharedMemory1); \
  aesenc((unsigned char *)&s3, &rc[rci + 7], sharedMemory1);


#define AES4_LAST(s3, rci) \
  aesenc((unsigned char *)&s3, &rc[rci + 2],sharedMemory1); \
  aesenc((unsigned char *)&s3, &rc[rci + 6], sharedMemory1); \



#define MIX2_EMU(s0, s1) \
  tmp = _mm_unpacklo_epi32_emu(s0, s1); \
  s1 = _mm_unpackhi_epi32_emu(s0, s1); \
  s0 = tmp;

#define MIX4(s0, s1, s2, s3) \
  tmp  = _mm_unpacklo_epi32_emu(s0, s1); \
  s0 = _mm_unpackhi_epi32_emu(s0, s1); \
  s1 = _mm_unpacklo_epi32_emu(s2, s3); \
  s2 = _mm_unpackhi_epi32_emu(s2, s3); \
  s3 = _mm_unpacklo_epi32_emu(s0, s2); \
  s0 = _mm_unpackhi_epi32_emu(s0, s2); \
  s2 = _mm_unpackhi_epi32_emu(s1, tmp); \
  s1 = _mm_unpacklo_epi32_emu(s1, tmp);

#define MIX4_LASTBUT1(s0, s1, s2, s3) \
  tmp  = _mm_unpacklo_epi32_emu(s0, s1); \
  s1 = _mm_unpacklo_epi32_emu(s2, s3); \
  s2 = _mm_unpackhi_epi32_emu(s1, tmp); 

#define saes_data(w) {\
    w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), w(0xc5),\
    w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), w(0xab), w(0x76),\
    w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), w(0x59), w(0x47), w(0xf0),\
    w(0xad), w(0xd4), w(0xa2), w(0xaf), w(0x9c), w(0xa4), w(0x72), w(0xc0),\
    w(0xb7), w(0xfd), w(0x93), w(0x26), w(0x36), w(0x3f), w(0xf7), w(0xcc),\
    w(0x34), w(0xa5), w(0xe5), w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15),\
    w(0x04), w(0xc7), w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a),\
    w(0x07), w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), w(0x75),\
    w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), w(0x5a), w(0xa0),\
    w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), w(0xe3), w(0x2f), w(0x84),\
    w(0x53), w(0xd1), w(0x00), w(0xed), w(0x20), w(0xfc), w(0xb1), w(0x5b),\
    w(0x6a), w(0xcb), w(0xbe), w(0x39), w(0x4a), w(0x4c), w(0x58), w(0xcf),\
    w(0xd0), w(0xef), w(0xaa), w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85),\
    w(0x45), w(0xf9), w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8),\
    w(0x51), w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), w(0xf5),\
    w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), w(0xf3), w(0xd2),\
    w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), w(0x97), w(0x44), w(0x17),\
    w(0xc4), w(0xa7), w(0x7e), w(0x3d), w(0x64), w(0x5d), w(0x19), w(0x73),\
    w(0x60), w(0x81), w(0x4f), w(0xdc), w(0x22), w(0x2a), w(0x90), w(0x88),\
    w(0x46), w(0xee), w(0xb8), w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb),\
    w(0xe0), w(0x32), w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c),\
    w(0xc2), w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), w(0x79),\
    w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), w(0x4e), w(0xa9),\
    w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), w(0x7a), w(0xae), w(0x08),\
    w(0xba), w(0x78), w(0x25), w(0x2e), w(0x1c), w(0xa6), w(0xb4), w(0xc6),\
    w(0xe8), w(0xdd), w(0x74), w(0x1f), w(0x4b), w(0xbd), w(0x8b), w(0x8a),\
    w(0x70), w(0x3e), w(0xb5), w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e),\
    w(0x61), w(0x35), w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e),\
    w(0xe1), w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), w(0x94),\
    w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), w(0x28), w(0xdf),\
    w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), w(0xe6), w(0x42), w(0x68),\
    w(0x41), w(0x99), w(0x2d), w(0x0f), w(0xb0), w(0x54), w(0xbb), w(0x16) }

#define SAES_WPOLY           0x011b

#define saes_b2w(b0, b1, b2, b3) (((uint32_t)(b3) << 24) | \
    ((uint32_t)(b2) << 16) | ((uint32_t)(b1) << 8) | (b0))

#define saes_f2(x)   ((x<<1) ^ (((x>>7) & 1) * SAES_WPOLY))
#define saes_f3(x)   (saes_f2(x) ^ x)
#define saes_h0(x)   (x)

#define saes_u0(p)   saes_b2w(saes_f2(p),          p,          p, saes_f3(p))
#define saes_u1(p)   saes_b2w(saes_f3(p), saes_f2(p),          p,          p)
#define saes_u2(p)   saes_b2w(         p, saes_f3(p), saes_f2(p),          p)
#define saes_u3(p)   saes_b2w(         p,          p, saes_f3(p), saes_f2(p))

static const __constant  uint32_t saes_table[4][256] = { saes_data(saes_u0), saes_data(saes_u1), saes_data(saes_u2), saes_data(saes_u3) };


uint32_t xor3x(uint a, uint b, uint c) {
	uint result;

	result = a^b^c;

	return result;
}

uint128m _mm_xor_si128_emu(uint128m a, uint128m b)
{

	return a ^ b;
}



uint128m _mm_clmulepi64_si128_emu(uint128m ai, uint128m bi, int imm)
{
	uint64_t a = ((uint64_t*)&ai)[0]; 

	uint64_t b = ((uint64_t*)&bi)[1];
	
	uint8_t  i; 

	uint64_t u[8];
	uint128m r;
	uint64_t tmp;

	u[0] = 0;  //000 x b
	u[1] = b;  //001 x b
	u[2] = u[1] << 1; //010 x b
	u[3] = u[2] ^ b;  //011 x b
	u[4] = u[2] << 1; //100 x b
	u[5] = u[4] ^ b;  //101 x b
	u[6] = u[3] << 1; //110 x b
	u[7] = u[6] ^ b;  //111 x b
					  //Multiply
	((uint64_t*)&r)[0] = u[a & 7]; //first window only affects lower word

	r.z = r.w = 0;
	#pragma unroll
	for (i = 3; i < 64; i += 3) {
		tmp = u[a >> i & 7];
	//	((uint64_t*)&r)[0] ^= tmp << i;
		r.x ^= (tmp << i) & 0xffffffff;
		r.y ^= ((tmp << i) & 0xffffffff00000000) >> 32;
	//	((uint64_t*)&r)[1] ^= tmp >> (64 - i);
		r.z ^= (tmp >> (64 - i)) & 0xffffffff;
		r.w ^= ((tmp >> (64 - i)) & 0xffffffff00000000) >> 32;
	}
#define LIMMY_R(x, y, z) ( x >> z | (y << (32 - z)))

	if ((bi.w ) & 0x80000000)
	{
		uint32_t t0 = LIMMY_R(ai.x, ai.y, 1);
		uint32_t t1 = ai.y >> 1;
		r.z ^= (t0 & 0xDB6DB6DB); //0, 21x 110
		r.w ^= (t1 & 0x36DB6DB6); //0x6DB6DB6DB6DB6DB6 -> 0x36DB6DB6DB6DB6DB after >>1
	}
	if ((bi.w ) &  0x40000000)
	{
		uint32_t t0 = LIMMY_R(ai.x, ai.y, 2);
		uint32_t t1 = ai.y >> 2;
		r.z ^= (t0 & 0x49249249); //0, 21x 100
		r.w ^= (t1 & 0x12492492); //0x4924924924924924 -> 0x1249249249249249 after >>2
	}

	return r;
}



uint128m _mm_clmulepi64_si128_emu2(uint128m ai)
{
	uint64_t a = ((uint64_t*)&ai)[1];

	//uint64_t b = 27 ;
	uint8_t  i; //window size s = 4,
				//uint64_t two_s = 16; //2^s
				//uint64_t smask = 15; //s 15
	uint8_t u[8];
	uint128m r;
	uint64_t tmp;
	//Precomputation

	//#pragma unroll
	u[0] = 0;  //000 x b
	u[1] = 27;  //001 x b
	u[2] = 54; // u[1] << 1; //010 x b
	u[3] = 45;  //011 x b
	u[4] = 108; //100 x b
	u[5] = 119;  //101 x b
	u[6] = 90; //110 x b
	u[7] = 65;  //111 x b
				//Multiply
	((uint64_t*)&r)[0] = u[a & 7]; //first window only affects lower word

	r.z = r.w = 0;
	//#pragma unroll
	for (i = 3; i < 64; i += 3) {
		tmp = u[a >> i & 7];
		((uint64_t*)&r)[0] ^= tmp << i;

		((uint64_t*)&r)[1] ^= tmp >> (64 - i);
	}

	return r;
}

#define _mm_load_si128_emu(p) (*(uint128m*)(p));

#define _mm_cvtsi128_si64_emu(p) (((int64_t *)&p)[0]);

#define _mm_cvtsi128_si32_emu(p) (((int32_t *)&a)[0]);


void _mm_unpackboth_epi32_emu(uint128m *a, uint128m *b)
{
	uint32_t value;

	value = a[0].z; a[0].z = a[0].y; a[0].y = value;
	value = a[0].y; a[0].y = b[0].x; b[0].x = value;
	value = b[0].z; b[0].z = a[0].w; a[0].w = value;
	value = b[0].y; b[0].y = a[0].w; a[0].w = value;

}

uint128m _mm_unpacklo_epi32_emu(uint128m a, uint128m b)
{
	a.z = a.y;
	a.y = b.x;
	a.w = b.y;
	return a;
}

uint128m _mm_unpackhi_epi32_emu(uint128m a, uint128m b)
{
	b.x = a.z;
	b.y = b.z;
	b.z = a.w;
	return b;
}



inline void aesenc(unsigned char *s, __global   uint128m *key, __local uint *t)
{
	uint128m x0 = ((uint128m*)s)[0];

	uint128m y0 = { 0,0,0,0 };
	y0.x ^= t[x0.x & 0xff]; x0.x >>= 8;
	y0.y ^= t[x0.y & 0xff]; x0.y >>= 8;
	y0.z ^= t[x0.z & 0xff]; x0.z >>= 8;
	y0.w ^= t[x0.w & 0xff]; x0.w >>= 8;
	t += 256;

	y0.x ^= t[x0.y & 0xff]; x0.y >>= 8;
	y0.y ^= t[x0.z & 0xff]; x0.z >>= 8;
	y0.z ^= t[x0.w & 0xff]; x0.w >>= 8;
	y0.w ^= t[x0.x & 0xff]; x0.x >>= 8;
	t += 256;

	y0.x ^= t[x0.z & 0xff]; x0.z >>= 8;
	y0.y ^= t[x0.w & 0xff]; x0.w >>= 8;
	y0.z ^= t[x0.x & 0xff]; x0.x >>= 8;
	y0.w ^= t[x0.y & 0xff]; x0.y >>= 8;

	t += 256;

	y0.x ^= t[x0.w];
	y0.y ^= t[x0.x];
	y0.z ^= t[x0.y];
	y0.w ^= t[x0.z];

	((uint128m*)s)[0] = _mm_xor_si128_emu(y0, key[0]);

}

inline void aesenc_loc(unsigned char *s, uint128m *key, __local uint *t)
{
	uint128m x0 = ((uint128m*)s)[0];

	uint128m y0 = { 0,0,0,0 };
	y0.x ^= t[x0.x & 0xff]; x0.x >>= 8;
	y0.y ^= t[x0.y & 0xff]; x0.y >>= 8;
	y0.z ^= t[x0.z & 0xff]; x0.z >>= 8;
	y0.w ^= t[x0.w & 0xff]; x0.w >>= 8;
	t += 256;

	y0.x ^= t[x0.y & 0xff]; x0.y >>= 8;
	y0.y ^= t[x0.z & 0xff]; x0.z >>= 8;
	y0.z ^= t[x0.w & 0xff]; x0.w >>= 8;
	y0.w ^= t[x0.x & 0xff]; x0.x >>= 8;
	t += 256;

	y0.x ^= t[x0.z & 0xff]; x0.z >>= 8;
	y0.y ^= t[x0.w & 0xff]; x0.w >>= 8;
	y0.z ^= t[x0.x & 0xff]; x0.x >>= 8;
	y0.w ^= t[x0.y & 0xff]; x0.y >>= 8;

	t += 256;

	y0.x ^= t[x0.w];
	y0.y ^= t[x0.x];
	y0.z ^= t[x0.y];
	y0.w ^= t[x0.z];

	((uint128m*)s)[0] = _mm_xor_si128_emu(y0, key[0]);

}

uint128m _mm_cvtsi32_si128_emu(uint32_t lo)
{
	uint128m result = { 0,0,0,0 };
	result.x = lo;
	return result;
}

uint128m _mm_cvtsi64_si128_emu(uint64_t lo)
{
	uint128m result = { 0,0,0,0 };
	((uint64_t *)&result)[0] = lo;
	//((uint64_t *)&result)[1] = 0;
	return result;
}
uint128m _mm_set_epi64x_emu(uint64_t hi, uint64_t lo)
{
	uint128m result;
	((uint64_t *)&result)[0] = lo;
	((uint64_t *)&result)[1] = hi;
	return result;
}
uint128m _mm_shuffle_epi8_emu(uint128m b)
{
	uint128m result = { 0,0,0,0 };
	uint128m M = { 0x2d361b00,0x415a776c,0xf5eec3d8,0x9982afb4 };
//#pragma unroll 16
	for (int i = 0; i < 16; i++)
	{
		if (((uint8_t *)&b)[i] & 0x80)
		{
			((uint8_t *)&result)[i] = 0;
		}
		else
		{
			((uint8_t *)&result)[i] = ((uint8_t *)&M)[((uint8_t *)&b)[i] & 0xf];
		}
	}

	return result;
}

uint128m _mm_srli_si128_emu(uint128m input, int imm8)
{
	//we can cheat here as its an 8 byte shift just copy the 64bits
	uint128m temp;
	((uint64_t*)&temp)[0] = ((uint64_t*)&input)[1];
	((uint64_t*)&temp)[1] = 0;
	return temp;
}

uint128m _mm_mulhrs_epi16_emu(uint128m _a, uint128m _b)
{
	int16_t result[8];

	int16_t *a = (int16_t*)&_a, *b = (int16_t*)&_b;

	for (int i = 0; i < 8; i++)
	{
		result[i] = (int16_t)((((int32_t)(a[i]) * (int32_t)(b[i])) + 0x4000) >> 15);
	}
	return *(uint128m *)result;
}

void case_0(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	 uint128m temp1 = prandex[0];

	 uint128m temp2 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));


	 uint128m add1 = _mm_xor_si128_emu(temp1, temp2);

	 uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);

	 uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	 uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);

	 uint128m temp12 = prand[0];
	prand[0] = tempa2;


	 uint128m temp22 = _mm_load_si128_emu(pbuf);
	 uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
	 uint128m clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod12, acc[0]);

	 uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
	 uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
	prandex[0] = tempb2;

}

void case_4(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	 uint128m temp1 = prand[0];
	 uint128m temp2 = _mm_load_si128_emu(pbuf);
	 uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	 uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);
	 uint128m clprod2 = _mm_clmulepi64_si128_emu(temp2, temp2, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod2, acc[0]);

	 uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	 uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);

	 uint128m temp12 = prandex[0];
	prandex[0] = tempa2;

	 uint128m temp22 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	 uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
	acc[0] = _mm_xor_si128_emu(add12, acc[0]);

	 uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
	 uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
	prand[0] = tempb2;
}

void case_8(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	 uint128m temp1 = prandex[0];
	 uint128m temp2 = _mm_load_si128_emu(pbuf);
	 uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	acc[0] = _mm_xor_si128_emu(add1, acc[0]);

	 uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	 uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);

	 uint128m temp12 = prand[0];
	prand[0] = tempa2;

	 uint128m temp22 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	 uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
	 uint128m clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod12, acc[0]);
	 uint128m clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod22, acc[0]);

	 uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
	 uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
	prandex[0] = tempb2;
}

void case_0c(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	 uint128m temp1 = prand[0];
	 uint128m temp2 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	 uint128m add1 = _mm_xor_si128_emu(temp1, temp2);

	// cannot be zero here
	 int32_t divisor = ((uint32_t*)&selector)[0];

	acc[0] = _mm_xor_si128_emu(add1, acc[0]);

	int64_t dividend = _mm_cvtsi128_si64_emu(acc[0]);
	int64_t tmpmod = dividend % divisor;
	 uint128m modulo = _mm_cvtsi32_si128_emu(tmpmod);
	acc[0] = _mm_xor_si128_emu(modulo, acc[0]);

	 uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	 uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);
	dividend &= 1;
	if (dividend)
	{
		 uint128m temp12 = prandex[0];
		prandex[0] = tempa2;

		 uint128m temp22 = _mm_load_si128_emu(pbuf);
		 uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
		 uint128m clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
		acc[0] = _mm_xor_si128_emu(clprod12, acc[0]);
		 uint128m clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
		acc[0] = _mm_xor_si128_emu(clprod22, acc[0]);

		 uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
		 uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
		prand[0] = tempb2;
	}
	else
	{
		 uint128m tempb3 = prandex[0];
		prandex[0] = tempa2;
		prand[0] = tempb3;
	}
}

void case_10(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc, uint128m *randomsource, uint32_t prand_idx, __local uint32_t *sharedMemory1)
{			// a few AES operations
	//uint128m rc[12];

	//rc[0] = prand[0];

	uint128m *rc = randomsource;

	uint128m tmp, temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	uint128m temp2 = _mm_load_si128_emu(pbuf);

	AES2_EMU_LOC(temp1, temp2, 0);
	MIX2_EMU(temp1, temp2);


	AES2_EMU_LOC(temp1, temp2, 4);
	MIX2_EMU(temp1, temp2);

	AES2_EMU_LOC(temp1, temp2, 8);
	MIX2_EMU(temp1, temp2);


	acc[0] = _mm_xor_si128_emu(temp1, acc[0]);
	acc[0] = _mm_xor_si128_emu(temp2, acc[0]);

	 uint128m tempa1 = prand[0];
	 uint128m tempa2 = _mm_mulhrs_epi16_emu(acc[0], tempa1);
	 uint128m tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

	 uint128m tempa4 = prandex[0];
	prandex[0] = tempa3;
	prand[0] = tempa4;
}

void case_14(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc, __global  uint128m *randomsource, uint32_t prand_idx, __local uint32_t *sharedMemory1)
{
	// we'll just call this one the monkins loop, inspired by Chris
	 uint128m *buftmp = pbuf - (((selector & 1) << 1) - 1);
	//	uint128m tmp; // used by MIX2

	uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
	__global  uint128m *rc = &randomsource[prand_idx];


	uint64_t aesround = 0;
	uint128m onekey, tmp;
	uint64_t loop_c;

	for (int i = 0; i<8; i++)
	{
		if (rounds <= 8) {
			loop_c = selector & ((uint64_t)0x10000000 << rounds);
			if (loop_c)
			{
				onekey = rc[0]; rc++; // _mm_load_si128_emu(rc++);
				 uint128m temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
				 uint128m add1 = _mm_xor_si128_emu(onekey, temp2);
				 uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
				acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);
			}
			else
			{
				onekey = rc[0]; rc++; // _mm_load_si128_emu(rc++);
				uint128m temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);

				 uint64_t roundidx = aesround++ << 2;
				AES2_EMU(onekey, temp2, roundidx);

				MIX2_EMU(onekey, temp2);

				acc[0] = _mm_xor_si128_emu(onekey, acc[0]);
				acc[0] = _mm_xor_si128_emu(temp2, acc[0]);

			}
		}
		(rounds--);
	}

	 uint128m tempa1 = (prand[0]);
	 uint128m tempa2 = _mm_mulhrs_epi16_emu(acc[0], tempa1);
	 uint128m tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

	 uint128m tempa4 = (prandex[0]);
	prandex[0] = tempa3;
	prand[0] = tempa4;
}

void case_18(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc, __global  uint128m *randomsource, uint32_t prand_idx, __local uint32_t *sharedMemory1)
{
	const uint128m *buftmp = pbuf - (((selector & 1) << 1) - 1);
	uint128m tmp; // used by MIX2

	uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
	__global  uint128m *rc = &randomsource[prand_idx];


	uint64_t aesround = 0;
	uint128m onekey;
	uint64_t loop_c;

	for (int i = 0; i<8; i++)
	{
		if (rounds <= 8) {
			loop_c = selector & ((uint64_t)0x10000000 << rounds);
			if (loop_c)
			{
				onekey = rc[0]; rc++; 
				const uint128m temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
				const uint128m add1 = _mm_xor_si128_emu(onekey, temp2);

				const int32_t divisor = (uint32_t)selector;
				const int64_t dividend = ((int64_t*)&add1)[0];
				uint128m modulo = { 0,0,0,0 }; ((int32_t*)&modulo)[0] = (dividend % divisor);
				acc[0] = _mm_xor_si128_emu(modulo , acc[0]);

			}
			else
			{
				onekey = rc[0]; rc++; 
				uint128m temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
				uint128m add1 = _mm_xor_si128_emu(onekey, temp2);
				uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
				uint128m clprod2 = _mm_mulhrs_epi16_emu(acc[0], clprod1);
				acc[0] = clprod2^ acc[0];
			}
		}
		(rounds--);
	}

	const uint128m tempa3 = (prandex[0]);
	const uint128m tempa4 = _mm_xor_si128_emu(tempa3, acc[0]);
	prandex[0] = tempa4;
	prand[0] = onekey;
}

void case_1c(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	 uint128m temp1 = _mm_load_si128_emu(pbuf);
	 uint128m temp2 = (prandex[0]);
	 uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	 uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);


	 uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp2);
	 uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp2);
	 uint128m tempa3 = (prand[0]);


	prand[0] = tempa2;

	acc[0] = _mm_xor_si128_emu(tempa3, acc[0]);

	 uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], tempa3);
	 uint128m tempb2 = _mm_xor_si128_emu(tempb1, tempa3);
	prandex[0] = tempb2;
}


#define m1 	rc[1] = randomsource[prand_idx + 1];\
rc[2] = randomsource[prand_idx + 2];\
rc[3] = randomsource[prand_idx + 3];\
rc[4] = randomsource[prand_idx + 4];\
rc[5] = randomsource[prand_idx + 5];\
rc[6] = randomsource[prand_idx + 6];\
rc[7] = randomsource[prand_idx + 7];\
rc[8] = randomsource[prand_idx + 8];\
rc[9] = randomsource[prand_idx + 9];\
rc[10] = randomsource[prand_idx + 10];\
rc[11] = randomsource[prand_idx + 11];\
rc[0] = prand;

#define m2 selector = _mm_cvtsi128_si64_emu(acc);\
prand_idx = ((acc.x >> 5) & 511);\
prandex_idx = ((acc.y) & 511);\
case_v = selector & 0x1cu;\
prand = randomsource[prand_idx];\
prandex = randomsource[prandex_idx];\
pbuf = buf + (acc.x & 3);


#define m3 	d_fix_r[i] = prand_idx;\
d_fix_rex[i] = prandex_idx;\
randomsource[prand_idx] = prand;\
randomsource[prandex_idx] = prandex;\
i++;

#define C0 	if (case_v == 0 && i != 32)\
{case_0(&prand, &prandex, pbuf, selector, &acc);\
	m3\
		m2}

#define C1 	if (case_v == 4 && i != 32)\
{case_4(&prand, &prandex, pbuf, selector, &acc);\
   m3\
   m2}



#define C2		if (case_v == 8 && i != 32)\
{case_8(&prand, &prandex, pbuf, selector, &acc);\
	m3\
		m2}\

#define C3		if (case_v == 0xc && i != 32)\
{	case_0c(&prand, &prandex, pbuf, selector, &acc);\
	m3\
		m2}\


#define C4  if (case_v == 0x10 && i != 32)\
{	m1\
		case_10(&prand, &prandex, pbuf, selector, &acc, rc, prand_idx, sharedMemory1);\
	m3\
		m2}\

#define C5 		 if (case_v == 0x14 && i != 32)\
{	case_14(&prand, &prandex, pbuf, selector, &acc, randomsource, prand_idx, sharedMemory1);\
	m3\
		m2}\

#define C6 		if (case_v == 0x18 && i != 32)\
{	case_18(&prand, &prandex, pbuf, selector, &acc, randomsource, prand_idx, sharedMemory1);\
	m3\
		m2}\

#define C7 		if (case_v == 0x1C && i != 32)\
{	case_1c(&prand, &prandex, pbuf, selector, &acc);\
	m3\
		m2}\




uint128m __verusclmulwithoutreduction64alignedrepeatgpu(__global uint128m * randomsource, uint128m *buf,
	__local uint32_t *sharedMemory1, __local uint16_t *d_fix_r, __local uint16_t *d_fix_rex)
{
	uint128m const *pbuf;
	//keyMask >>= 4;
	uint128m acc = randomsource[513];
	buf[0] = buf[0] ^ buf[2];
	buf[1] = buf[1] ^ buf[3];

	// divide key mask by 32 from bytes to uint128m

	uint16_t prand_idx, prandex_idx;
	uint64_t selector;
	uint128m prand;
	uint128m prandex;
	prand_idx = ((acc.x >> 5) & 511);
	prandex_idx = ((acc.y) & 511);

	prand = randomsource[prand_idx];
	prandex = randomsource[prandex_idx];
		uint8_t case_v;
	//#pragma unroll
	uint32_t i = 0;
		selector = _mm_cvtsi128_si64_emu(acc); 
		case_v = selector & 0x1cu; 
		pbuf = buf + (acc.x & 3); 
	do
	{
		uint128m rc[12];
		C5
		C0
		C1
		C2
		C3
		C4
		C5
		C6
		C7



	} while (i != 32);

	return acc;
}


uint32_t haraka512_port_keyed2222(const uint128m *in, __global uint128m *rc, __local uint32_t *sharedMemory1)
{
	uint128m s1, s2, s3, s4, tmp;

	s1 = in[0];
	s2 = in[1];
	s3 = in[2];
	s4 = in[3];

	AES4(s1, s2, s3, s4, 0);
	MIX4(s1, s2, s3, s4);

	AES4(s1, s2, s3, s4, 8);
	MIX4(s1, s2, s3, s4);

	AES4(s1, s2, s3, s4, 16);
	MIX4(s1, s2, s3, s4);

	AES4(s1, s2, s3, s4, 24);
	MIX4_LASTBUT1(s1, s2, s3, s4);

	AES4_LAST(s3, 32);

	return s3.z ^ in[3].y;

}


ulong precompReduction64(uint128m A) {


	//static const uint128m M = { 0x2d361b00,0x415a776c,0xf5eec3d8,0x9982afb4 };
	// const uint128m tmp = { 27 };
	// A.z = 0;
	//tmp.x = 27u;
	uint128m Q2 = _mm_clmulepi64_si128_emu2(A);
	uint128m Q3 = _mm_shuffle_epi8_emu(_mm_srli_si128_emu(Q2, 8));

	//uint128m Q4 = _mm_xor_si128_emu(Q2, A);
	uint128m final;
	final.x = xor3x(A.x, Q2.x, Q3.x);
	final.y = xor3x(A.y, Q2.y, Q3.y);

	return _mm_cvtsi128_si64_emu(final);/// WARNING: HIGH 64 BITS SHOULD BE ASSUMED TO CONTAIN GARBAGE
}


__kernel __attribute__((reqd_work_group_size(64, 1, 1)))
__kernel void verus_gpu_hash(__constant uint *startNonce,
	__constant uint128m *blockhash_half, __global uint128m *data_keylarge, __global uint64_t *acc,
	__global uint32_t *d_fix_r, __global uint32_t *d_fix_rex)
{
	uint thread = get_global_id(0);
	uint128m mid; 
	uint128m s[4];
	
	__private uint lid = get_local_id(0);
	uint nounce = startNonce[0] + thread;

	__local  uint sharedMemory1[4][256];
	__local  uint16_t sharedrand[THREADS * 32];
	__local  uint16_t sharedrandex[THREADS * 32];

	__global uint128m *pkey = &data_keylarge[0] + ((thread) * VERUS_KEY_SIZE128);

	s[0] = blockhash_half[0];
	s[1] = blockhash_half[1];
	s[2] = blockhash_half[2];
	s[3] = blockhash_half[3];


	sharedMemory1[0][get_local_id(0)] = saes_table[0][get_local_id(0)];// copy sbox to shared mem
	sharedMemory1[0][get_local_id(0) + 64] = saes_table[0][get_local_id(0) + 64];// copy sbox to shared mem
	sharedMemory1[0][get_local_id(0) + 128] = saes_table[0][get_local_id(0) + 128];// copy sbox to shared mem
	sharedMemory1[0][get_local_id(0) + 192] = saes_table[0][get_local_id(0) + 192];// copy sbox to shared mem

	sharedMemory1[1][get_local_id(0)] = saes_table[1][get_local_id(0)];// copy sbox to shared mem
	sharedMemory1[1][get_local_id(0) + 64] = saes_table[1][get_local_id(0) + 64];// copy sbox to shared mem
	sharedMemory1[1][get_local_id(0) + 128] = saes_table[1][get_local_id(0) + 128];// copy sbox to shared mem
	sharedMemory1[1][get_local_id(0) + 192] = saes_table[1][get_local_id(0) + 192];// copy sbox to shared mem

	sharedMemory1[2][get_local_id(0)] = saes_table[2][get_local_id(0)];// copy sbox to shared mem
	sharedMemory1[2][get_local_id(0) + 64] = saes_table[2][get_local_id(0) + 64];// copy sbox to shared mem
	sharedMemory1[2][get_local_id(0) + 128] = saes_table[2][get_local_id(0) + 128];// copy sbox to shared mem
	sharedMemory1[2][get_local_id(0) + 192] = saes_table[2][get_local_id(0) + 192];// copy sbox to shared mem

	sharedMemory1[3][get_local_id(0)] = saes_table[3][get_local_id(0)];// copy sbox to shared mem
	sharedMemory1[3][get_local_id(0) + 64] = saes_table[3][get_local_id(0) + 64];// copy sbox to shared mem
	sharedMemory1[3][get_local_id(0) + 128] = saes_table[3][get_local_id(0) + 128];// copy sbox to shared mem
	sharedMemory1[3][get_local_id(0) + 192] = saes_table[3][get_local_id(0) + 192];// copy sbox to shared mem

	mem_fence(CLK_LOCAL_MEM_FENCE); //sync sharedmem
	((uint *)&s)[8] = nounce;

	mid = __verusclmulwithoutreduction64alignedrepeatgpu(pkey, s, sharedMemory1[0],
		&sharedrand[lid *32], &sharedrandex[lid * 32]);
	mid.x ^= 0x00010000;


	acc[thread] = precompReduction64(mid);;
	

	for (int i = 0; i < 32; i++)
	{
		d_fix_r[(thread * 32) + i] = sharedrand[(lid * 32) + i];
		d_fix_rex[(thread * 32) + i] = sharedrandex[(lid * 32) + i];
	}

};

__kernel __attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void verus_gpu_final(__constant uint *startNonce, __constant uint128m *blockhash_half, 
	__global uint *target, __global uint *resNonce, __global uint128m *data_keylarge,
	__global ulong *acc)
{
	uint thread = get_global_id(0);

	uint hash; uint128m s[4];
	ulong acc_loc = acc[thread];
	uint nounce = startNonce[0] + thread;
	__local  uint sharedMemory1[4][256];
	__private uint lid = get_local_id(0);
	__global uint128m *pkey = &data_keylarge[0] + ((thread)* VERUS_KEY_SIZE128);

	s[0] = blockhash_half[0];
	s[1] = blockhash_half[1];
	s[2] = blockhash_half[2];
	s[3] = blockhash_half[3];
	sharedMemory1[0][get_local_id(0)] = saes_table[0][get_local_id(0)];// copy sbox to shared mem

	sharedMemory1[1][get_local_id(0)] = saes_table[1][get_local_id(0)];// copy sbox to shared mem

	sharedMemory1[2][get_local_id(0)] = saes_table[2][get_local_id(0)];// copy sbox to shared mem


	sharedMemory1[3][get_local_id(0)] = saes_table[3][get_local_id(0)];// copy sbox to shared mem

	mem_fence(CLK_LOCAL_MEM_FENCE);

	((uint*)&s)[8] = nounce;
	memcpy(((uchar*)&s) + 47, (uchar*)&acc_loc, 8);
	memcpy(((uchar*)&s) + 55, (uchar*)&acc_loc, 8);
	memcpy(((uchar*)&s) + 63, (uchar*)&acc_loc, 1);

	acc_loc &= 511;

	hash = haraka512_port_keyed2222(s, &pkey[acc_loc], sharedMemory1[0]);

	if (hash < target[7]) {
		resNonce[0] = nounce;
	}

	

};

__kernel __attribute__((reqd_work_group_size(128, 1, 1)))
__kernel void verus_key(__constant uint128m * d_key_input, __global uint128m *data_keylarge)
{

	uint thread = get_local_id(0);
	uint block = get_group_id(0);

	data_keylarge[(block * VERUS_KEY_SIZE128) + thread] = d_key_input[thread];
	data_keylarge[(block * VERUS_KEY_SIZE128) + thread + 128] = d_key_input[thread + 128];
	data_keylarge[(block * VERUS_KEY_SIZE128) + thread + 256] = d_key_input[thread + 256];
	data_keylarge[(block * VERUS_KEY_SIZE128) + thread + 384] = d_key_input[thread + 384];
	if (thread < 40)
		data_keylarge[(block * VERUS_KEY_SIZE128) + thread + 512] = d_key_input[thread + 512];

}

__kernel __attribute__((reqd_work_group_size(32, 1, 1)))
__kernel void verus_extra_gpu_fix(__constant uint128m *d_key_input, __global uint128m *data_keylarge,
	__global uint32_t *d_fix_r, __global uint32_t *d_fix_rex)
{

	uint thread = get_local_id(0);
	uint block = get_group_id(0);
	data_keylarge[(block * VERUS_KEY_SIZE128) + d_fix_r[(block * 32) + thread]] = d_key_input[d_fix_r[(block * 32) + thread]];
	data_keylarge[(block * VERUS_KEY_SIZE128) + d_fix_rex[(block * 32) + thread]] = d_key_input[d_fix_rex[(block * 32) + thread]];

}