#pragma OPENCL EXTENSION cl_nvidia_printf : enable
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
  aesenc_last((unsigned char *)&s3, &rc[rci + 6], sharedMemory1); \


#define TRUNCSTORE(out, s4) \
  *(uint*)(out + 28) = s4.y;

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


__constant uint sbox[] = {
	0x7b777c63, 0xc56f6bf2, 0x2b670130, 0x76abd7fe, 0x7dc982ca, 0xf04759fa, 0xafa2d4ad, 0xc072a49c, 0x2693fdb7, 0xccf73f36, 0xf1e5a534, 0x1531d871, 0xc323c704, 0x9a059618, 0xe2801207, 0x75b227eb, 0x1a2c8309, 0xa05a6e1b, 0xb3d63b52, 0x842fe329, 0xed00d153, 0x5bb1fc20, 0x39becb6a, 0xcf584c4a, 0xfbaaefd0, 0x85334d43, 0x7f02f945, 0xa89f3c50, 0x8f40a351, 0xf5389d92, 0x21dab6bc, 0xd2f3ff10, 0xec130ccd, 0x1744975f, 0x3d7ea7c4, 0x73195d64, 0xdc4f8160, 0x88902a22, 0x14b8ee46, 0xdb0b5ede, 0x0a3a32e0, 0x5c240649, 0x62acd3c2, 0x79e49591, 0x6d37c8e7, 0xa94ed58d, 0xeaf4566c, 0x08ae7a65, 0x2e2578ba, 0xc6b4a61c, 0x1f74dde8, 0x8a8bbd4b, 0x66b53e70, 0x0ef60348, 0xb9573561, 0x9e1dc186, 0x1198f8e1, 0x948ed969, 0xe9871e9b, 0xdf2855ce, 0x0d89a18c, 0x6842e6bf, 0x0f2d9941, 0x16bb54b0,
	0x7b777c63, 0xc56f6bf2, 0x2b670130, 0x76abd7fe, 0x7dc982ca, 0xf04759fa, 0xafa2d4ad, 0xc072a49c, 0x2693fdb7, 0xccf73f36, 0xf1e5a534, 0x1531d871, 0xc323c704, 0x9a059618, 0xe2801207, 0x75b227eb, 0x1a2c8309, 0xa05a6e1b, 0xb3d63b52, 0x842fe329, 0xed00d153, 0x5bb1fc20, 0x39becb6a, 0xcf584c4a, 0xfbaaefd0, 0x85334d43, 0x7f02f945, 0xa89f3c50, 0x8f40a351, 0xf5389d92, 0x21dab6bc, 0xd2f3ff10, 0xec130ccd, 0x1744975f, 0x3d7ea7c4, 0x73195d64, 0xdc4f8160, 0x88902a22, 0x14b8ee46, 0xdb0b5ede, 0x0a3a32e0, 0x5c240649, 0x62acd3c2, 0x79e49591, 0x6d37c8e7, 0xa94ed58d, 0xeaf4566c, 0x08ae7a65, 0x2e2578ba, 0xc6b4a61c, 0x1f74dde8, 0x8a8bbd4b, 0x66b53e70, 0x0ef60348, 0xb9573561, 0x9e1dc186, 0x1198f8e1, 0x948ed969, 0xe9871e9b, 0xdf2855ce, 0x0d89a18c, 0x6842e6bf, 0x0f2d9941, 0x16bb54b0,
	0x7b777c63, 0xc56f6bf2, 0x2b670130, 0x76abd7fe, 0x7dc982ca, 0xf04759fa, 0xafa2d4ad, 0xc072a49c, 0x2693fdb7, 0xccf73f36, 0xf1e5a534, 0x1531d871, 0xc323c704, 0x9a059618, 0xe2801207, 0x75b227eb, 0x1a2c8309, 0xa05a6e1b, 0xb3d63b52, 0x842fe329, 0xed00d153, 0x5bb1fc20, 0x39becb6a, 0xcf584c4a, 0xfbaaefd0, 0x85334d43, 0x7f02f945, 0xa89f3c50, 0x8f40a351, 0xf5389d92, 0x21dab6bc, 0xd2f3ff10, 0xec130ccd, 0x1744975f, 0x3d7ea7c4, 0x73195d64, 0xdc4f8160, 0x88902a22, 0x14b8ee46, 0xdb0b5ede, 0x0a3a32e0, 0x5c240649, 0x62acd3c2, 0x79e49591, 0x6d37c8e7, 0xa94ed58d, 0xeaf4566c, 0x08ae7a65, 0x2e2578ba, 0xc6b4a61c, 0x1f74dde8, 0x8a8bbd4b, 0x66b53e70, 0x0ef60348, 0xb9573561, 0x9e1dc186, 0x1198f8e1, 0x948ed969, 0xe9871e9b, 0xdf2855ce, 0x0d89a18c, 0x6842e6bf, 0x0f2d9941, 0x16bb54b0,
	0x7b777c63, 0xc56f6bf2, 0x2b670130, 0x76abd7fe, 0x7dc982ca, 0xf04759fa, 0xafa2d4ad, 0xc072a49c, 0x2693fdb7, 0xccf73f36, 0xf1e5a534, 0x1531d871, 0xc323c704, 0x9a059618, 0xe2801207, 0x75b227eb, 0x1a2c8309, 0xa05a6e1b, 0xb3d63b52, 0x842fe329, 0xed00d153, 0x5bb1fc20, 0x39becb6a, 0xcf584c4a, 0xfbaaefd0, 0x85334d43, 0x7f02f945, 0xa89f3c50, 0x8f40a351, 0xf5389d92, 0x21dab6bc, 0xd2f3ff10, 0xec130ccd, 0x1744975f, 0x3d7ea7c4, 0x73195d64, 0xdc4f8160, 0x88902a22, 0x14b8ee46, 0xdb0b5ede, 0x0a3a32e0, 0x5c240649, 0x62acd3c2, 0x79e49591, 0x6d37c8e7, 0xa94ed58d, 0xeaf4566c, 0x08ae7a65, 0x2e2578ba, 0xc6b4a61c, 0x1f74dde8, 0x8a8bbd4b, 0x66b53e70, 0x0ef60348, 0xb9573561, 0x9e1dc186, 0x1198f8e1, 0x948ed969, 0xe9871e9b, 0xdf2855ce, 0x0d89a18c, 0x6842e6bf, 0x0f2d9941, 0x16bb54b0
};


uint32_t xor3x(uint a, uint b, uint c) {
	uint result;

	result = a^b^c;

	return result;
}

uint128m _mm_xor_si128_emu(uint128m a, uint128m b)
{
	uint128m c;
	c.x = a.x ^ b.x;
	c.y = a.y ^ b.y;
	c.z = a.z ^ b.z;
	c.w = a.w ^ b.w;
	return c;
}

#define XT4(x) ((((x) << 1) & 0xfefefefe) ^ ((((x) >> 7) & 0x1010101) * 0x1b))

// -----------------------------
// VERUSHASH V2.0
// -----------------------------


uint128m _mm_clmulepi64_si128_emu(uint128m ai, uint128m bi, int imm)
{
	uint64_t a = ((uint64_t*)&ai)[0]; // (0xffffffffull & ai.x) | ((0x00000000ffffffffull & ai.y) << 32);//+ (imm & 1));

	uint64_t b = ((uint64_t*)&bi)[1]; // (0xffffffffull & bi.z) | ((0x00000000ffffffffull & bi.w) << 32);

	uint8_t  i; //window size s = 4,
				//uint64_t two_s = 16; //2^s
				//uint64_t smask = 15; //s 15
	uint64_t u[8];
	uint128m r;
	uint64_t tmp;
	//Precomputation

	//#pragma unroll
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
	//#pragma unroll
	for (i = 3; i < 64; i += 3) {
		tmp = u[a >> i & 7];
		((uint64_t*)&r)[0] ^= tmp << i;

		((uint64_t*)&r)[1] ^= tmp >> (64 - i);
	}

	if ((bi.w) & 0x80000000)
	{
		uint32_t t0 = ((uint64_t*)&ai)[0] >> 1;
		uint32_t t1 = ai.y >> 1;
		r.z ^= (t0 & 0xDB6DB6DB); //0, 21x 110
		r.w ^= (t1 & 0x36DB6DB6); //0x6DB6DB6DB6DB6DB6 -> 0x36DB6DB6DB6DB6DB after >>1
	}
	if ((bi.w) & 0x40000000)
	{
		uint32_t t0 = ((uint64_t*)&ai)[0] >> 1; //LIMMY_R(ai.x, ai.y, 2);
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

void aesenc_last(unsigned char *s, __global uint128m *rk, __local uint32_t *sharedMemory1)
{

	uint32_t  v[4];

	((uchar*)&v[0])[0] = sharedMemory1[s[0]];
	((uchar*)&v[0])[7] = sharedMemory1[s[1]];
	((uchar*)&v[0])[10] = sharedMemory1[s[2]];
	((uchar*)&v[0])[13] = sharedMemory1[s[3]];
	((uchar*)&v[0])[1] = sharedMemory1[s[4]];
	((uchar*)&v[0])[4] = sharedMemory1[s[5]];
	((uchar*)&v[0])[11] = sharedMemory1[s[6]];
	((uchar*)&v[0])[14] = sharedMemory1[s[7]];
	((uchar*)&v[0])[2] = sharedMemory1[s[8]];
	((uchar*)&v[0])[5] = sharedMemory1[s[9]];
	((uchar*)&v[0])[8] = sharedMemory1[s[10]];
	((uchar*)&v[0])[15] = sharedMemory1[s[11]];
	((uchar*)&v[0])[3] = sharedMemory1[s[12]];
	((uchar*)&v[0])[6] = sharedMemory1[s[13]];
	((uchar*)&v[0])[9] = sharedMemory1[s[14]];
	((uchar*)&v[0])[12] = sharedMemory1[s[15]];

	uint32_t t = v[0];
	uint32_t w = v[0] ^ v[1];
	uint32_t u; // = w ^ v[2] ^ v[3];
	u = xor3x(w, v[2], v[3]);
	v[0] = xor3x(v[0], u, XT4(w));
	v[1] = xor3x(v[1], u, XT4(v[1] ^ v[2]));
	v[2] = xor3x(v[2], u, XT4(v[2] ^ v[3]));
	v[3] = xor3x(v[3], u, XT4(v[3] ^ t));

	s[8] = ((uint8_t*)&v[0])[2];
	s[9] = ((uint8_t*)&v[0])[6];
	s[10] = ((uint8_t*)&v[0])[10];
	s[11] = ((uint8_t*)&v[0])[14];

	((uint128m*)&s[0])[0].z = ((uint32_t*)&s[0])[2] ^ rk[0].z;
}

void aesenc(unsigned char *s, __global uint128m *rk, __local uint32_t *sharedMemory1)
{
	uint32_t  v[4];
	//const uint128m rk2 = ((uint128m*)&rk[0])[0];

	((uchar*)&v[0])[0] = sharedMemory1[s[0]];
	((uchar*)&v[0])[7] = sharedMemory1[s[1]];
	((uchar*)&v[0])[10] = sharedMemory1[s[2]];
	((uchar*)&v[0])[13] = sharedMemory1[s[3]];
	((uchar*)&v[0])[1] = sharedMemory1[s[4]];
	((uchar*)&v[0])[4] = sharedMemory1[s[5]];
	((uchar*)&v[0])[11] = sharedMemory1[s[6]];
	((uchar*)&v[0])[14] = sharedMemory1[s[7]];
	((uchar*)&v[0])[2] = sharedMemory1[s[8]];
	((uchar*)&v[0])[5] = sharedMemory1[s[9]];
	((uchar*)&v[0])[8] = sharedMemory1[s[10]];
	((uchar*)&v[0])[15] = sharedMemory1[s[11]];
	((uchar*)&v[0])[3] = sharedMemory1[s[12]];
	((uchar*)&v[0])[6] = sharedMemory1[s[13]];
	((uchar*)&v[0])[9] = sharedMemory1[s[14]];
	((uchar*)&v[0])[12] = sharedMemory1[s[15]];

	uint32_t t = v[0];
	uint32_t w = v[0] ^ v[1];
	uint32_t u; // = w ^ v[2] ^ v[3];
	u = xor3x(w, v[2], v[3]);
	v[0] = xor3x(v[0], u, XT4(w));
	v[1] = xor3x(v[1], u, XT4(v[1] ^ v[2]));
	v[2] = xor3x(v[2], u, XT4(v[2] ^ v[3]));
	v[3] = xor3x(v[3], u, XT4(v[3] ^ t));


	s[0] = ((uint8_t*)&v[0])[0];
	s[1] = ((uint8_t*)&v[0])[4];
	s[2] = ((uint8_t*)&v[0])[8];
	s[3] = ((uint8_t*)&v[0])[12];
	s[4] = ((uint8_t*)&v[0])[1];
	s[5] = ((uint8_t*)&v[0])[5];

	s[6] = ((uint8_t*)&v[0])[9];
	s[7] = ((uint8_t*)&v[0])[13];
	s[8] = ((uint8_t*)&v[0])[2];
	s[9] = ((uint8_t*)&v[0])[6];
	s[10] = ((uint8_t*)&v[0])[10];
	s[11] = ((uint8_t*)&v[0])[14];
	s[12] = ((uint8_t*)&v[0])[3];
	s[13] = ((uint8_t*)&v[0])[7];
	s[14] = ((uint8_t*)&v[0])[11];
	s[15] = ((uint8_t*)&v[0])[15];

	((uint*)&s[0])[0] = ((uint32_t*)&s[0])[0] ^ rk[0].x;
	((uint*)&s[0])[1] = ((uint32_t*)&s[0])[1] ^ rk[0].y;
	((uint*)&s[0])[2] = ((uint32_t*)&s[0])[2] ^ rk[0].z;
	((uint*)&s[0])[3] = ((uint32_t*)&s[0])[3] ^ rk[0].w;


}
#define AES2_EMU2(s0, s1, rci) \
  aesenc4((unsigned char *)&s0, (unsigned char *)&s1, &rc[rci], sharedMemory1); 

void aesenc4(unsigned char *s1, unsigned char *s2, __global uint128m *rk, __local uint32_t *sharedMemory1)
{
	uint32_t v[4];
	uint32_t t, w, u;

	((uchar*)&v[0])[0] = sharedMemory1[s1[0]];
	((uchar*)&v[0])[7] = sharedMemory1[s1[1]];
	((uchar*)&v[0])[10] = sharedMemory1[s1[2]];
	((uchar*)&v[0])[13] = sharedMemory1[s1[3]];
	((uchar*)&v[0])[1] = sharedMemory1[s1[4]];
	((uchar*)&v[0])[4] = sharedMemory1[s1[5]];
	((uchar*)&v[0])[11] = sharedMemory1[s1[6]];
	((uchar*)&v[0])[14] = sharedMemory1[s1[7]];
	((uchar*)&v[0])[2] = sharedMemory1[s1[8]];
	((uchar*)&v[0])[5] = sharedMemory1[s1[9]];
	((uchar*)&v[0])[8] = sharedMemory1[s1[10]];
	((uchar*)&v[0])[15] = sharedMemory1[s1[11]];
	((uchar*)&v[0])[3] = sharedMemory1[s1[12]];
	((uchar*)&v[0])[6] = sharedMemory1[s1[13]];
	((uchar*)&v[0])[9] = sharedMemory1[s1[14]];
	((uchar*)&v[0])[12] = sharedMemory1[s1[15]];

	t = v[0];
	w = v[0] ^ v[1];

	u = xor3x(w, v[2], v[3]);
	v[0] = xor3x(v[0], u, XT4(w));
	v[1] = xor3x(v[1], u, XT4(v[1] ^ v[2]));
	v[2] = xor3x(v[2], u, XT4(v[2] ^ v[3]));
	v[3] = xor3x(v[3], u, XT4(v[3] ^ t));


	s1[0] = ((uint8_t*)&v[0])[0];
	s1[1] = ((uint8_t*)&v[0])[4];
	s1[2] = ((uint8_t*)&v[0])[8];
	s1[3] = ((uint8_t*)&v[0])[12];
	s1[4] = ((uint8_t*)&v[0])[1];
	s1[5] = ((uint8_t*)&v[0])[5];

	s1[6] = ((uint8_t*)&v[0])[9];
	s1[7] = ((uint8_t*)&v[0])[13];
	s1[8] = ((uint8_t*)&v[0])[2];
	s1[9] = ((uint8_t*)&v[0])[6];
	s1[10] = ((uint8_t*)&v[0])[10];
	s1[11] = ((uint8_t*)&v[0])[14];
	s1[12] = ((uint8_t*)&v[0])[3];
	s1[13] = ((uint8_t*)&v[0])[7];
	s1[14] = ((uint8_t*)&v[0])[11];
	s1[15] = ((uint8_t*)&v[0])[15];

	((uint*)&s1[0])[0] = ((uint32_t*)&s1[0])[0] ^ rk[0].x;
	((uint*)&s1[0])[1] = ((uint32_t*)&s1[0])[1] ^ rk[0].y;
	((uint*)&s1[0])[2] = ((uint32_t*)&s1[0])[2] ^ rk[0].z;
	((uint*)&s1[0])[3] = ((uint32_t*)&s1[0])[3] ^ rk[0].w;

	((uchar*)&v[0])[0] = sharedMemory1[s2[0]];
	((uchar*)&v[0])[7] = sharedMemory1[s2[1]];
	((uchar*)&v[0])[10] = sharedMemory1[s2[2]];
	((uchar*)&v[0])[13] = sharedMemory1[s2[3]];
	((uchar*)&v[0])[1] = sharedMemory1[s2[4]];
	((uchar*)&v[0])[4] = sharedMemory1[s2[5]];
	((uchar*)&v[0])[11] = sharedMemory1[s2[6]];
	((uchar*)&v[0])[14] = sharedMemory1[s2[7]];
	((uchar*)&v[0])[2] = sharedMemory1[s2[8]];
	((uchar*)&v[0])[5] = sharedMemory1[s2[9]];
	((uchar*)&v[0])[8] = sharedMemory1[s2[10]];
	((uchar*)&v[0])[15] = sharedMemory1[s2[11]];
	((uchar*)&v[0])[3] = sharedMemory1[s2[12]];
	((uchar*)&v[0])[6] = sharedMemory1[s2[13]];
	((uchar*)&v[0])[9] = sharedMemory1[s2[14]];
	((uchar*)&v[0])[12] = sharedMemory1[s2[15]];

	t = v[0];
	w = v[0] ^ v[1];

	u = xor3x(w, v[2], v[3]);
	v[0] = xor3x(v[0], u, XT4(w));
	v[1] = xor3x(v[1], u, XT4(v[1] ^ v[2]));
	v[2] = xor3x(v[2], u, XT4(v[2] ^ v[3]));
	v[3] = xor3x(v[3], u, XT4(v[3] ^ t));


	s2[0] = ((uint8_t*)&v[0])[0];
	s2[1] = ((uint8_t*)&v[0])[4];
	s2[2] = ((uint8_t*)&v[0])[8];
	s2[3] = ((uint8_t*)&v[0])[12];
	s2[4] = ((uint8_t*)&v[0])[1];
	s2[5] = ((uint8_t*)&v[0])[5];

	s2[6] = ((uint8_t*)&v[0])[9];
	s2[7] = ((uint8_t*)&v[0])[13];
	s2[8] = ((uint8_t*)&v[0])[2];
	s2[9] = ((uint8_t*)&v[0])[6];
	s2[10] = ((uint8_t*)&v[0])[10];
	s2[11] = ((uint8_t*)&v[0])[14];
	s2[12] = ((uint8_t*)&v[0])[3];
	s2[13] = ((uint8_t*)&v[0])[7];
	s2[14] = ((uint8_t*)&v[0])[11];
	s2[15] = ((uint8_t*)&v[0])[15];

	((uint*)&s2[0])[0] = ((uint32_t*)&s2[0])[0] ^ rk[1].x;
	((uint*)&s2[0])[1] = ((uint32_t*)&s2[0])[1] ^ rk[1].y;
	((uint*)&s2[0])[2] = ((uint32_t*)&s2[0])[2] ^ rk[1].z;
	((uint*)&s2[0])[3] = ((uint32_t*)&s2[0])[3] ^ rk[1].w;


	((uchar*)&v[0])[0] = sharedMemory1[s1[0]];
	((uchar*)&v[0])[7] = sharedMemory1[s1[1]];
	((uchar*)&v[0])[10] = sharedMemory1[s1[2]];
	((uchar*)&v[0])[13] = sharedMemory1[s1[3]];
	((uchar*)&v[0])[1] = sharedMemory1[s1[4]];
	((uchar*)&v[0])[4] = sharedMemory1[s1[5]];
	((uchar*)&v[0])[11] = sharedMemory1[s1[6]];
	((uchar*)&v[0])[14] = sharedMemory1[s1[7]];
	((uchar*)&v[0])[2] = sharedMemory1[s1[8]];
	((uchar*)&v[0])[5] = sharedMemory1[s1[9]];
	((uchar*)&v[0])[8] = sharedMemory1[s1[10]];
	((uchar*)&v[0])[15] = sharedMemory1[s1[11]];
	((uchar*)&v[0])[3] = sharedMemory1[s1[12]];
	((uchar*)&v[0])[6] = sharedMemory1[s1[13]];
	((uchar*)&v[0])[9] = sharedMemory1[s1[14]];
	((uchar*)&v[0])[12] = sharedMemory1[s1[15]];

	t = v[0];
	w = v[0] ^ v[1];

	u = xor3x(w, v[2], v[3]);
	v[0] = xor3x(v[0], u, XT4(w));
	v[1] = xor3x(v[1], u, XT4(v[1] ^ v[2]));
	v[2] = xor3x(v[2], u, XT4(v[2] ^ v[3]));
	v[3] = xor3x(v[3], u, XT4(v[3] ^ t));


	s1[0] = ((uint8_t*)&v[0])[0];
	s1[1] = ((uint8_t*)&v[0])[4];
	s1[2] = ((uint8_t*)&v[0])[8];
	s1[3] = ((uint8_t*)&v[0])[12];
	s1[4] = ((uint8_t*)&v[0])[1];
	s1[5] = ((uint8_t*)&v[0])[5];

	s1[6] = ((uint8_t*)&v[0])[9];
	s1[7] = ((uint8_t*)&v[0])[13];
	s1[8] = ((uint8_t*)&v[0])[2];
	s1[9] = ((uint8_t*)&v[0])[6];
	s1[10] = ((uint8_t*)&v[0])[10];
	s1[11] = ((uint8_t*)&v[0])[14];
	s1[12] = ((uint8_t*)&v[0])[3];
	s1[13] = ((uint8_t*)&v[0])[7];
	s1[14] = ((uint8_t*)&v[0])[11];
	s1[15] = ((uint8_t*)&v[0])[15];

	((uint*)&s1[0])[0] = ((uint32_t*)&s1[0])[0] ^ rk[2].x;
	((uint*)&s1[0])[1] = ((uint32_t*)&s1[0])[1] ^ rk[2].y;
	((uint*)&s1[0])[2] = ((uint32_t*)&s1[0])[2] ^ rk[2].z;
	((uint*)&s1[0])[3] = ((uint32_t*)&s1[0])[3] ^ rk[2].w;

	((uchar*)&v[0])[0] = sharedMemory1[s2[0]];
	((uchar*)&v[0])[7] = sharedMemory1[s2[1]];
	((uchar*)&v[0])[10] = sharedMemory1[s2[2]];
	((uchar*)&v[0])[13] = sharedMemory1[s2[3]];
	((uchar*)&v[0])[1] = sharedMemory1[s2[4]];
	((uchar*)&v[0])[4] = sharedMemory1[s2[5]];
	((uchar*)&v[0])[11] = sharedMemory1[s2[6]];
	((uchar*)&v[0])[14] = sharedMemory1[s2[7]];
	((uchar*)&v[0])[2] = sharedMemory1[s2[8]];
	((uchar*)&v[0])[5] = sharedMemory1[s2[9]];
	((uchar*)&v[0])[8] = sharedMemory1[s2[10]];
	((uchar*)&v[0])[15] = sharedMemory1[s2[11]];
	((uchar*)&v[0])[3] = sharedMemory1[s2[12]];
	((uchar*)&v[0])[6] = sharedMemory1[s2[13]];
	((uchar*)&v[0])[9] = sharedMemory1[s2[14]];
	((uchar*)&v[0])[12] = sharedMemory1[s2[15]];

	t = v[0];
	w = v[0] ^ v[1];

	u = xor3x(w, v[2], v[3]);
	v[0] = xor3x(v[0], u, XT4(w));
	v[1] = xor3x(v[1], u, XT4(v[1] ^ v[2]));
	v[2] = xor3x(v[2], u, XT4(v[2] ^ v[3]));
	v[3] = xor3x(v[3], u, XT4(v[3] ^ t));



	s2[0] = ((uint8_t*)&v[0])[0];
	s2[1] = ((uint8_t*)&v[0])[4];
	s2[2] = ((uint8_t*)&v[0])[8];
	s2[3] = ((uint8_t*)&v[0])[12];
	s2[4] = ((uint8_t*)&v[0])[1];
	s2[5] = ((uint8_t*)&v[0])[5];

	s2[6] = ((uint8_t*)&v[0])[9];
	s2[7] = ((uint8_t*)&v[0])[13];
	s2[8] = ((uint8_t*)&v[0])[2];
	s2[9] = ((uint8_t*)&v[0])[6];
	s2[10] = ((uint8_t*)&v[0])[10];
	s2[11] = ((uint8_t*)&v[0])[14];
	s2[12] = ((uint8_t*)&v[0])[3];
	s2[13] = ((uint8_t*)&v[0])[7];
	s2[14] = ((uint8_t*)&v[0])[11];
	s2[15] = ((uint8_t*)&v[0])[15];

	((uint*)&s2[0])[0] = ((uint32_t*)&s2[0])[0] ^ rk[3].x;
	((uint*)&s2[0])[1] = ((uint32_t*)&s2[0])[1] ^ rk[3].y;
	((uint*)&s2[0])[2] = ((uint32_t*)&s2[0])[2] ^ rk[3].z;
	((uint*)&s2[0])[3] = ((uint32_t*)&s2[0])[3] ^ rk[3].w;

	_mm_unpackboth_epi32_emu((uint128m*)s1, (uint128m*)s2);
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
	uint128m result;
	uint128m M = { 0x2d361b00,0x415a776c,0xf5eec3d8,0x9982afb4 };
#pragma unroll
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

	int32_t po;
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
	const uint128m temp1 = prandex[0];

	const uint128m temp2 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));


	const uint128m add1 = _mm_xor_si128_emu(temp1, temp2);

	const uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);

	const uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	const uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);

	const uint128m temp12 = prand[0];
	prand[0] = tempa2;


	const uint128m temp22 = _mm_load_si128_emu(pbuf);
	const uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
	const uint128m clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod12, acc[0]);

	const uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
	const uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
	prandex[0] = tempb2;

}

void case_4(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	const uint128m temp1 = prand[0];
	const uint128m temp2 = _mm_load_si128_emu(pbuf);
	const uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	const uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);
	const uint128m clprod2 = _mm_clmulepi64_si128_emu(temp2, temp2, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod2, acc[0]);

	const uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	const uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);

	const uint128m temp12 = prandex[0];
	prandex[0] = tempa2;

	const uint128m temp22 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	const uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
	acc[0] = _mm_xor_si128_emu(add12, acc[0]);

	const uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
	const uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
	prand[0] = tempb2;
}

void case_8(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	const uint128m temp1 = prandex[0];
	const uint128m temp2 = _mm_load_si128_emu(pbuf);
	const uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	acc[0] = _mm_xor_si128_emu(add1, acc[0]);

	const uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	const uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);

	const uint128m temp12 = prand[0];
	prand[0] = tempa2;

	const uint128m temp22 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	const uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
	const uint128m clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod12, acc[0]);
	const uint128m clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod22, acc[0]);

	const uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
	const uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
	prandex[0] = tempb2;
}

void case_0c(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	const uint128m temp1 = prand[0];
	const uint128m temp2 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	const uint128m add1 = _mm_xor_si128_emu(temp1, temp2);

	// cannot be zero here
	const int32_t divisor = ((uint32_t*)&selector)[0];

	acc[0] = _mm_xor_si128_emu(add1, acc[0]);

	int64_t dividend = _mm_cvtsi128_si64_emu(acc[0]);
	int64_t tmpmod = dividend % divisor;
	const uint128m modulo = _mm_cvtsi32_si128_emu(tmpmod);
	acc[0] = _mm_xor_si128_emu(modulo, acc[0]);

	const uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp1);
	const uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp1);
	dividend &= 1;
	if (dividend)
	{
		const uint128m temp12 = prandex[0];
		prandex[0] = tempa2;

		const uint128m temp22 = _mm_load_si128_emu(pbuf);
		const uint128m add12 = _mm_xor_si128_emu(temp12, temp22);
		const uint128m clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
		acc[0] = _mm_xor_si128_emu(clprod12, acc[0]);
		const uint128m clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
		acc[0] = _mm_xor_si128_emu(clprod22, acc[0]);

		const uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], temp12);
		const uint128m tempb2 = _mm_xor_si128_emu(tempb1, temp12);
		prand[0] = tempb2;
	}
	else
	{
		const uint128m tempb3 = prandex[0];
		prandex[0] = tempa2;
		prand[0] = tempb3;
	}
}

void case_10(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc, __global uint128m *randomsource, uint32_t prand_idx, __local uint32_t *sharedMemory1)
{			// a few AES operations
	//uint128m rc[12];

	//rc[0] = prand[0];

	__global uint128m *rc = &randomsource[prand_idx];
	/*rc[1] = randomsource[prand_idx + 1];
	rc[2] = randomsource[prand_idx + 2];
	rc[3] = randomsource[prand_idx + 3];
	rc[4] = randomsource[prand_idx + 4];
	rc[5] = randomsource[prand_idx + 5];
	rc[6] = randomsource[prand_idx + 6];
	rc[7] = randomsource[prand_idx + 7];
	rc[8] = randomsource[prand_idx + 8];
	rc[9] = randomsource[prand_idx + 9];
	rc[10] = randomsource[prand_idx + 10];
	rc[11] = randomsource[prand_idx + 11];
	//	uint128m tmp;
	*/
	uint128m temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	uint128m temp2 = _mm_load_si128_emu(pbuf);

	AES2_EMU2(temp1, temp2, 0);
	//	MIX2_EMU(temp1, temp2);


	AES2_EMU2(temp1, temp2, 4);
	//	MIX2_EMU(temp1, temp2);

	AES2_EMU2(temp1, temp2, 8);
	//	MIX2_EMU(temp1, temp2);


	acc[0] = _mm_xor_si128_emu(temp1, acc[0]);
	acc[0] = _mm_xor_si128_emu(temp2, acc[0]);

	const uint128m tempa1 = prand[0];
	const uint128m tempa2 = _mm_mulhrs_epi16_emu(acc[0], tempa1);
	const uint128m tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

	const uint128m tempa4 = prandex[0];
	prandex[0] = tempa3;
	prand[0] = tempa4;
}
void case_14(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc, __global uint128m *randomsource, uint32_t prand_idx, __local uint32_t *sharedMemory1)
{
	// we'll just call this one the monkins loop, inspired by Chris
	const uint128m *buftmp = pbuf - (((selector & 1) << 1) - 1);
	//	uint128m tmp; // used by MIX2

	uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
	__global uint128m *rc = &randomsource[prand_idx];


	uint64_t aesround = 0;
	uint128m onekey;
	uint64_t loop_c;

	for (int i = 0; i<8; i++)
	{
		if (rounds <= 8) {
			loop_c = selector & (0x10000000 << rounds);
			if (loop_c)
			{
				onekey = rc[0]; rc++; // _mm_load_si128_emu(rc++);
				const uint128m temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
				const uint128m add1 = _mm_xor_si128_emu(onekey, temp2);
				const uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
				acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);
			}
			else
			{
				onekey = rc[0]; rc++; // _mm_load_si128_emu(rc++);
				uint128m temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);

				const uint64_t roundidx = aesround++ << 2;
				AES2_EMU2(onekey, temp2, roundidx);

				//	MIX2_EMU(onekey, temp2);

				acc[0] = _mm_xor_si128_emu(onekey, acc[0]);
				acc[0] = _mm_xor_si128_emu(temp2, acc[0]);

			}
		}
		(rounds--);
	}

	const uint128m tempa1 = (prand[0]);
	const uint128m tempa2 = _mm_mulhrs_epi16_emu(acc[0], tempa1);
	const uint128m tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

	const uint128m tempa4 = (prandex[0]);
	prandex[0] = tempa3;
	prand[0] = tempa4;
}

void case_18(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	const uint128m temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
	const uint128m temp2 = (prand[0]);
	const uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	const uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);

	const uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp2);
	const uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp2);

	const uint128m tempb3 = (prandex[0]);
	prandex[0] = tempa2;
	prand[0] = tempb3;
}

void case_1c(uint128m *prand, uint128m *prandex, const  uint128m *pbuf,
	uint64_t selector, uint128m *acc)
{
	const uint128m temp1 = _mm_load_si128_emu(pbuf);
	const uint128m temp2 = (prandex[0]);
	const uint128m add1 = _mm_xor_si128_emu(temp1, temp2);
	const uint128m clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
	acc[0] = _mm_xor_si128_emu(clprod1, acc[0]);


	const uint128m tempa1 = _mm_mulhrs_epi16_emu(acc[0], temp2);
	const uint128m tempa2 = _mm_xor_si128_emu(tempa1, temp2);
	const uint128m tempa3 = (prand[0]);


	prand[0] = tempa2;

	acc[0] = _mm_xor_si128_emu(tempa3, acc[0]);

	const uint128m tempb1 = _mm_mulhrs_epi16_emu(acc[0], tempa3);
	const uint128m tempb2 = _mm_xor_si128_emu(tempb1, tempa3);
	prandex[0] = tempb2;
}



uint128m __verusclmulwithoutreduction64alignedrepeatgpu(__global uint128m * randomsource, const  uint128m *buf,
	__local uint32_t *sharedMemory1, __local uint32_t *d_fix_r, __local uint32_t *d_fix_rex)
{
	uint128m const *pbuf;
	//keyMask >>= 4;
	uint128m acc = randomsource[513];

#ifdef GPU_DEBUGGY
	if (nounce == 0)
	{
		printf("[GPU]BUF ito verusclmulithout        : ");
		for (int i = 0; i < 64; i++)
			printf("%02x", ((uint8_t*)&buf[0])[i]);
		printf("\n");
		printf("[GPU]KEy ito verusclmulithout        : ");
		for (int e = 0; e < 64; e++)
			printf("%02x", ((uint8_t*)&randomsource[0])[e]);
		printf("\n");
		printf("[GPU]ACC ito verusclmulithout        : ");
		for (int i = 0; i < 16; i++)
			printf("%02x", ((uint8_t*)&acc)[i]);
		printf("\n");
	}
#endif	
	// divide key mask by 32 from bytes to uint128m

	uint16_t prand_idx, prandex_idx;
	uint64_t selector;
	uint128m prand;
	uint128m prandex;
	prand_idx = ((acc.x >> 5) & 511);
	prandex_idx = ((acc.y) & 511);

	prand = randomsource[prand_idx];
	prandex = randomsource[prandex_idx];
	//#pragma unroll
	for (uint8_t i = 0; i < 32; i++)
	{

		selector = _mm_cvtsi128_si64_emu(acc);
		prand_idx = ((acc.x >> 5) & 511);
		prandex_idx = ((acc.y) & 511);

		if (i > 0) {

			// get two random locations in the key, which will be mutated and swapped
			prand = randomsource[prand_idx];
			prandex = randomsource[prandex_idx];
		}

		pbuf = buf + (acc.x & 3);
		uint8_t case_v;
		case_v = selector & 0x1cu;


		if (case_v == 0)
		{
			case_0(&prand, &prandex, pbuf, selector, &acc);
		}
		if (case_v == 4)
		{
			case_4(&prand, &prandex, pbuf, selector, &acc);
		}
		if (case_v == 8)
		{
			case_8(&prand, &prandex, pbuf, selector, &acc);

		}
		if (case_v == 0xc)
		{
			case_0c(&prand, &prandex, pbuf, selector, &acc);

		}

		if (case_v == 0x10)
		{
			case_10(&prand, &prandex, pbuf, selector, &acc, randomsource, prand_idx, sharedMemory1);


		}

		if (case_v == 0x14)
		{
			case_14(&prand, &prandex, pbuf, selector, &acc, randomsource, prand_idx, sharedMemory1);

		}


		if (case_v == 0x18)
		{
			case_18(&prand, &prandex, pbuf, selector, &acc);

		}
		if (case_v == 0x1c)
		{
			case_1c(&prand, &prandex, pbuf, selector, &acc);
		}
		d_fix_r[i] = prand_idx;
		d_fix_rex[i] = prandex_idx;
		randomsource[prand_idx] = prand;
		randomsource[prandex_idx] = prandex;

	}

	return acc;
}


uint32_t haraka512_port_keyed2222(const unsigned char *in, __global uint128m *rc, __local uint32_t *sharedMemory1)
{
	uint128m s1, s2, s3, s4, tmp;

	s1 = ((uint128m*)&in[0])[0];
	s2 = ((uint128m*)&in[0])[1];
	s3 = ((uint128m*)&in[0])[2];
	s4 = ((uint128m*)&in[0])[3];

	AES4(s1, s2, s3, s4, 0);
	MIX4(s1, s2, s3, s4);

	AES4(s1, s2, s3, s4, 8);
	MIX4(s1, s2, s3, s4);

	AES4(s1, s2, s3, s4, 16);
	MIX4(s1, s2, s3, s4);

	AES4(s1, s2, s3, s4, 24);
	MIX4_LASTBUT1(s1, s2, s3, s4);


	AES4_LAST(s3, 32);

	return s3.z ^ ((uint128m*)&in[0])[3].y;

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
__kernel void verus_gpu_hash(__global uint *startNonce, __global uint128m *d_key_input,
	__global uint *blockhash_half, __global uint128m *d_mid, __global uint *d_fix_r, __global uint *d_fix_rex)
{
	const uint thread = get_global_id(0) & 0xffffffff;
	uint128m mid; // , biddy[VERUS_KEY_SIZE128];
	uint128m s[4];

	const uint nounce = startNonce[0] + thread;

	__local  uint sharedMemory1[THREADS];
	__local  uint sharedrand[32 * THREADS];
	__local  uint sharedrandex[32 * THREADS];
	__private uint lid = get_local_id(0);
	//uint32_t save_rand[32] = { 0 };
	//uint32_t save_randex[32] = { 0 };

	s[0] = blockhash_half[0];
	s[1] = blockhash_half[1];
	s[2] = blockhash_half[2];
	s[3] = blockhash_half[3];


	sharedMemory1[lid] = sbox[lid];// copy sbox to shared mem
	mem_fence(CLK_LOCAL_MEM_FENCE);
	((uint *)&s)[8] = nounce;

	mid = __verusclmulwithoutreduction64alignedrepeatgpu(&d_key_input[VERUS_KEY_SIZE128 * thread], s, sharedMemory1, sharedrand + (lid * 32), sharedrandex + (lid * 32));
	mid.x ^= 0x00010000;
	d_mid[thread] = mid;

#pragma unroll
	for (int i = 0; i < 32; i++)
	{
		d_fix_r[(thread * 32) + i] = sharedrand[(lid * 32) + i];
		d_fix_rex[(thread * 32) + i] = sharedrandex[(lid * 32) + i];
	}
}


__kernel __attribute__((reqd_work_group_size(256, 1, 1)))
__kernel void verus_gpu_final(__global uint *startNonce, __global  uint128m *d_key_input,
	__global uint128m *blockhash_half, __global uint128m *d_mid, __global  uint *resNonce, __global  uint *ptarget)
{
	const uint thread = get_global_id(0) & 0xffffffff;
	ulong acc = precompReduction64(d_mid[thread]);;
	//uint128m wizz = d_mid[thread];

	const uint nounce = startNonce[0] + thread;
	uint hash;

	uint128m s[4];
	__local uint sharedMemory1[256];
	__private uint lid = get_local_id(0);


	sharedMemory1[lid] = sbox[lid];// copy sbox to shared mem
	s[0] = blockhash_half[0];
	s[1] = blockhash_half[1];
	s[2] = blockhash_half[2];
	s[3] = blockhash_half[3];
	mem_fence(CLK_LOCAL_MEM_FENCE);

	((uint*)&s)[8] = nounce;
	memcpy(((uchar*)&s) + 47, (uchar*)&acc, 8);
	memcpy(((uchar*)&s) + 55, (uchar*)&acc, 8);
	memcpy(((uchar*)&s) + 63, (uchar*)&acc, 1);
	//uint64_t mask = 8191 >> 4;
	acc &= 511;

	//haraka512_port_keyed((unsigned char*)hash, (const unsigned char*)s, (const unsigned char*)(biddy + mask), sharedMemory1, nounce);
	hash = haraka512_port_keyed2222((const uchar*)s, (&d_key_input[VERUS_KEY_SIZE128 * thread] + acc), sharedMemory1);

	if (hash < ptarget[7]) {

		resNonce[0] = nounce;

	}


};

__kernel __attribute__((reqd_work_group_size(128, 1, 1)))
__kernel void verus_extra_gpu_prepare(__global uint128m * vkey, __global uint128m *d_key_input)
{
	__private uint idx = get_local_id(0);
	__private uint gid = get_group_id(0);

	d_key_input[(gid * VERUS_KEY_SIZE128) + idx] = vkey[idx];
	d_key_input[(gid * VERUS_KEY_SIZE128) + idx + 128] = vkey[idx + 128];
	d_key_input[(gid * VERUS_KEY_SIZE128) + idx + 256] = vkey[idx + 256];
	d_key_input[(gid * VERUS_KEY_SIZE128) + idx + 384] = vkey[idx + 384];
	if (idx < 40)
		d_key_input[(gid * VERUS_KEY_SIZE128) + idx + 512] = vkey[idx + 512];

}

__kernel __attribute__((reqd_work_group_size(32, 1, 1)))
__kernel void verus_extra_gpu_fix(__global uint128m *d_key_input, __global uint *d_fix_r, __global uint *d_fix_rex, __global uint128m * vkey)
{
	__private uint idx = get_local_id(0);
	__private uint gid = get_group_id(0);
	d_key_input[(gid * VERUS_KEY_SIZE128) + d_fix_r[(gid * 32) + idx]] = vkey[d_fix_r[(gid * 32) + idx]];
	d_key_input[(gid * VERUS_KEY_SIZE128) + d_fix_rex[(gid * 32) + idx]] = vkey[d_fix_rex[(gid * 32) + idx]];

}