#ifdef WIN32
#pragma comment(lib, "winmm.lib")
#define _CRT_RAND_S 
#endif

#define _GNU_SOURCE	1/* memrchr */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "haraka_portable.h"

#include <errno.h>
#include <CL/cl.h>
#include "blake.h"
#include "sha256.h"

#ifdef WIN32

#undef _UNICODE // @mrb quick patch to make win getopt work

#include <Winsock2.h>
#include <io.h>
#include <BaseTsd.h>
#include "windows/gettimeofday.h"
#include "windows/getopt.h"
#include "windows/memrchr.h"

typedef SSIZE_T ssize_t;

#define open _open
#define read _read
#define write _write
#define close _close
#define snprintf _snprintf

#else

#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include "_kernel.h"

#endif

typedef uint8_t		uchar;
typedef uint32_t	uint;
#ifdef NVIDIA
#include "param-nvidia.h"
#else
#include "param.h"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define MIN(A, B)	(((A) < (B)) ? (A) : (B))
#define MAX(A, B)	(((A) > (B)) ? (A) : (B))
#define VERUS_KEY_SIZE 8832
#define VERUS_KEY_SIZE128 552
#define VERUS_WORKSIZE 0x8000
#define MAIN_THREADS 64

static u128 *data_key = NULL;


int         verbose = 0;
uint32_t	show_encoded = 0;
uint64_t	nr_nonces = 1;
uint32_t	do_list_devices = 0;
uint32_t	gpu_to_use = 0;
uint32_t	mining = 0;
uint32_t    blocks = 1;
uint32_t    gthreads = 256;
struct timeval kern_avg_run_time;
int amd_flag = 0;
const char *source = NULL;
size_t source_len;
const char *binary = NULL;
size_t binary_len;
uint32_t init_gpu[16] = { 0 };

typedef struct  debug_s
{
	uint32_t    dropped_coll;
	uint32_t    dropped_stor;
}               debug_t;

void debug(const char *fmt, ...)
{
	va_list     ap;
	if (!verbose)
		return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list     ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void fatal(const char *fmt, ...)
{
	va_list     ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

uint64_t parse_num(char *str)
{
	char	*endptr;
	uint64_t	n;
	n = strtoul(str, &endptr, 0);
	if (endptr == str || *endptr)
		fatal("'%s' is not a valid number\n", str);
	return n;
}

uint64_t now(void)
{
	struct timeval	tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

void show_time(uint64_t t0)
{
	uint64_t            t1;
	t1 = now();
	fprintf(stderr, "Elapsed time: %.1f msec\n", (t1 - t0) / 1e3);
}

#ifndef WIN32
void set_blocking_mode(int fd, int block)
{

	int	f;
	if (-1 == (f = fcntl(fd, F_GETFL)))
		fatal("fcntl F_GETFL: %s\n", strerror(errno));
	if (-1 == fcntl(fd, F_SETFL, block ? (f & ~O_NONBLOCK) : (f | O_NONBLOCK)))
		fatal("fcntl F_SETFL: %s\n", strerror(errno));
}
#endif

void randomize(void *p, ssize_t l)
{
#ifndef WIN32
	const char	*fname = "/dev/urandom";
	int		fd;
	ssize_t	ret;
	if (-1 == (fd = open(fname, O_RDONLY)))
		fatal("open %s: %s\n", fname, strerror(errno));
	if (-1 == (ret = read(fd, p, l)))
		fatal("read %s: %s\n", fname, strerror(errno));
	if (ret != l)
		fatal("%s: short read %d bytes out of %d\n", fname, ret, l);
	if (-1 == close(fd))
		fatal("close %s: %s\n", fname, strerror(errno));
#else
	for (int i = 0; i < l; i++) {
		unsigned int ui;
		rand_s(&ui);
		((uint8_t *)p)[i] = ui & 0xff;
	}
#endif
}

struct timeval time_diff(struct timeval start, struct timeval end)
{
	struct timeval temp;
	if ((end.tv_usec - start.tv_usec)<0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_usec = 1000000 + end.tv_usec - start.tv_usec;
	}
	else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_usec = end.tv_usec - start.tv_usec;
	}
	return temp;
}

cl_mem check_clCreateBuffer(cl_context ctx, cl_mem_flags flags, size_t size,
	void *host_ptr)
{
	cl_int	status;
	cl_mem	ret;
	ret = clCreateBuffer(ctx, flags, size, host_ptr, &status);
	if (status != CL_SUCCESS || !ret)
		fatal("clCreateBuffer (%d)\n", status);
	return ret;
}

void check_clSetKernelArg(cl_kernel k, cl_uint a_pos, cl_mem *a)
{
	cl_int	status;
	status = clSetKernelArg(k, a_pos, sizeof(*a), a);
	if (status != CL_SUCCESS)
		fatal("clSetKernelArg (%d)\n", status);
}

void check_clEnqueueNDRangeKernel(cl_command_queue queue, cl_kernel k, cl_uint
	work_dim, const size_t *global_work_offset, const size_t
	*global_work_size, const size_t *local_work_size, cl_uint
	num_events_in_wait_list, const cl_event *event_wait_list, cl_event
	*event)
{
	cl_uint	status;
	status = clEnqueueNDRangeKernel(queue, k, work_dim, global_work_offset,
		global_work_size, local_work_size, num_events_in_wait_list,
		event_wait_list, event);
	if (status != CL_SUCCESS)
		fatal("clEnqueueNDRangeKernel (%d)\n", status);
}

void check_clEnqueueReadBuffer(cl_command_queue queue, cl_mem buffer, cl_bool
	blocking_read, size_t offset, size_t size, void *ptr, cl_uint
	num_events_in_wait_list, const cl_event *event_wait_list, cl_event
	*event)
{
	cl_int	status;
	status = clEnqueueReadBuffer(queue, buffer, blocking_read, offset,
		size, ptr, num_events_in_wait_list, event_wait_list, event);
	if (status != CL_SUCCESS)
		fatal("clEnqueueReadBuffer (%d)\n", status);
}

void hexdump(uint8_t *a, uint32_t a_len)
{
	for (uint32_t i = 0; i < a_len; i++)
		fprintf(stderr, "%02x", a[i]);
}

char *s_hexdump(const void *_a, uint32_t a_len)
{
	const uint8_t	*a = _a;
	static char		buf[4096];
	uint32_t		i;
	for (i = 0; i < a_len && i + 2 < sizeof(buf); i++)
		sprintf(buf + i * 2, "%02x", a[i]);
	buf[i * 2] = 0;
	return buf;
}

uint8_t hex2val(const char *base, size_t off)
{
	const char          c = base[off];
	if (c >= '0' && c <= '9')           return c - '0';
	else if (c >= 'a' && c <= 'f')      return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')      return 10 + c - 'A';
	fatal("Invalid hex char at offset %d: ...%d...\n", off, c);
	return 0;
}



void load_file(const char *fname, char **dat, size_t *dat_len, int ignore_error)
{
	struct stat	st;
	int		fd;
	ssize_t	ret;
	if (-1 == (fd = open(fname, O_RDONLY | O_BINARY))) {
		if (ignore_error)
			return;
		fatal("%s: %s\n", fname, strerror(errno));
	}
	if (fstat(fd, &st))
		fatal("fstat: %s: %s\n", fname, strerror(errno));
	*dat_len = st.st_size;
	if (!(*dat = (char *)malloc(*dat_len + 1)))
		fatal("malloc: %s\n", strerror(errno));
	ret = read(fd, *dat, *dat_len);
	if (ret < 0)
		fatal("read: %s: %s\n", fname, strerror(errno));
	if ((size_t)ret != *dat_len)
		fatal("%s: partial read\n", fname);
	if (close(fd))
		fatal("close: %s: %s\n", fname, strerror(errno));
	(*dat)[*dat_len] = 0;
}

void get_program_build_log(cl_program program, cl_device_id device)
{
	cl_int		status;
	size_t		ret = 0;

	size_t len = 0;

	ret = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
	char *buffer = calloc(len, sizeof(char));
	ret = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, len, buffer, NULL);

	if (status == CL_SUCCESS)
		fatal("clGetProgramBuildInfo (%d)\n", status);
	printf("%s\n", buffer);

}

void dump(const char *fname, void *data, size_t len)
{
	int			fd;
	ssize_t		ret;
	if (-1 == (fd = open(fname, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0666)))
		fatal("%s: %s\n", fname, strerror(errno));
	ret = write(fd, data, len);
	if (ret == -1)
		fatal("write: %s: %s\n", fname, strerror(errno));
	if ((size_t)ret != len)
		fatal("%s: partial write\n", fname);
	if (-1 == close(fd))
		fatal("close: %s: %s\n", fname, strerror(errno));
}

void get_program_bins(cl_program program)
{
	cl_int		status;
	size_t		sizes;
	unsigned char	*p;
	size_t		ret = 0;
	status = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
		sizeof(sizes),	// size_t param_value_size
		&sizes,		// void *param_value
		&ret);		// size_t *param_value_size_ret
	if (status != CL_SUCCESS)
		fatal("clGetProgramInfo(sizes) (%d)\n", status);
	if (ret != sizeof(sizes))
		fatal("clGetProgramInfo(sizes) did not fill sizes (%d)\n", status);
	debug("Program binary size is %zd bytes\n", sizes);
	p = (unsigned char *)malloc(sizes);
	status = clGetProgramInfo(program, CL_PROGRAM_BINARIES,
		sizeof(p),	// size_t param_value_size
		&p,		// void *param_value
		&ret);	// size_t *param_value_size_ret
	if (status != CL_SUCCESS)
		fatal("clGetProgramInfo (%d)\n", status);
	dump("dump.co", p, sizes);
	debug("program: %02x%02x%02x%02x...\n", p[0], p[1], p[2], p[3]);
}

void print_platform_info(cl_platform_id plat)
{
	char	name[1024];
	size_t	len = 0;
	int		status;
	status = clGetPlatformInfo(plat, CL_PLATFORM_NAME, sizeof(name), &name,
		&len);
	if (status != CL_SUCCESS)
		fatal("clGetPlatformInfo (%d)\n", status);
	printf("Devices on platform \"%s\":\n", name);
	fflush(stdout);
}

int is_platform_amd(cl_platform_id plat)
{
	char	name[1024];
	size_t	len = 0;
	int		status;
	status = clGetPlatformInfo(plat, CL_PLATFORM_NAME, sizeof(name), &name,
		&len);
	if (status != CL_SUCCESS)
		fatal("clGetPlatformInfo (%d)\n", status);
	return strncmp(name, "AMD Accelerated Parallel Processing", len) == 0;
}

void print_device_info(unsigned i, cl_device_id d)
{
	char	name[1024];
	size_t	len = 0;
	int		status;
	status = clGetDeviceInfo(d, CL_DEVICE_NAME, sizeof(name), &name, &len);
	if (status != CL_SUCCESS)
		fatal("clGetDeviceInfo (%d)\n", status);
	printf("  ID %d: %s\n", i, name);
	fflush(stdout);
}

/////************VERUS2.0******************

void clmul64(uint64_t a, uint64_t b, uint64_t* r)
{
	uint8_t s = 4, i; //window size
	uint64_t two_s = 1 << s; //2^s
	uint64_t smask = two_s - 1; //s 1 bits
	uint64_t u[16];
	uint64_t tmp;
	uint64_t ifmask;
	//Precomputation
	u[0] = 0;
	u[1] = b;
	for (i = 2; i < two_s; i += 2) {
		u[i] = u[i >> 1] << 1; //even indices: left shift
		u[i + 1] = u[i] ^ b; //odd indices: xor b
	}
	//Multiply
	r[0] = u[a & smask]; //first window only affects lower word
	r[1] = 0;
	for (i = s; i < 64; i += s) {
		tmp = u[a >> i & smask];
		r[0] ^= tmp << i;
		r[1] ^= tmp >> (64 - i);
	}
	//Repair
	uint64_t m = 0xEEEEEEEEEEEEEEEE; //s=4 => 16 times 1110
	for (i = 1; i < s; i++) {
		tmp = ((a & m) >> i);
		m &= m << 1; //shift mask to exclude all bit j': j' mod s = i
		ifmask = -((b >> (64 - i)) & 1); //if the (64-i)th bit of b is 1
		r[1] ^= (tmp & ifmask);
	}
}

u128 _mm_clmulepi64_si128_emu(const __m128i a, const __m128i b, int imm)
{
	uint64_t result[2];
	clmul64(*((uint64_t*)&a + (imm & 1)), *((uint64_t*)&b + ((imm & 0x10) >> 4)), result);


	return *(__m128i *)result;
}

u128 _mm_mulhrs_epi16_emu(__m128i _a, __m128i _b)
{
	int16_t result[8];
	int16_t *a = (int16_t*)&_a, *b = (int16_t*)&_b;
	for (int i = 0; i < 8; i++)
	{
		result[i] = (int16_t)((((int32_t)(a[i]) * (int32_t)(b[i])) + 0x4000) >> 15);
	}

	return *(__m128i *)result;
}

inline u128 _mm_set_epi64x_emu(uint64_t hi, uint64_t lo)
{
	__m128i result;
	((uint64_t *)&result)[0] = lo;
	((uint64_t *)&result)[1] = hi;
	return result;
}

inline u128 _mm_cvtsi64_si128_emu(uint64_t lo)
{
	__m128i result;
	((uint64_t *)&result)[0] = lo;
	((uint64_t *)&result)[1] = 0;
	return result;
}

inline int64_t _mm_cvtsi128_si64_emu(__m128i a)
{
	return *(int64_t *)&a;
}

inline int32_t _mm_cvtsi128_si32_emu(__m128i a)
{
	return *(int32_t *)&a;
}

inline u128 _mm_cvtsi32_si128_emu(uint32_t lo)
{
	__m128i result;
	((uint32_t *)&result)[0] = lo;
	((uint32_t *)&result)[1] = 0;
	((uint64_t *)&result)[1] = 0;

	return result;
}

typedef unsigned char u_char;

u128 _mm_setr_epi8_emu(u_char c0, u_char c1, u_char c2, u_char c3, u_char c4, u_char c5, u_char c6, u_char c7, u_char c8, u_char c9, u_char c10, u_char c11, u_char c12, u_char c13, u_char c14, u_char c15)
{
	__m128i result;
	((uint8_t *)&result)[0] = c0;
	((uint8_t *)&result)[1] = c1;
	((uint8_t *)&result)[2] = c2;
	((uint8_t *)&result)[3] = c3;
	((uint8_t *)&result)[4] = c4;
	((uint8_t *)&result)[5] = c5;
	((uint8_t *)&result)[6] = c6;
	((uint8_t *)&result)[7] = c7;
	((uint8_t *)&result)[8] = c8;
	((uint8_t *)&result)[9] = c9;
	((uint8_t *)&result)[10] = c10;
	((uint8_t *)&result)[11] = c11;
	((uint8_t *)&result)[12] = c12;
	((uint8_t *)&result)[13] = c13;
	((uint8_t *)&result)[14] = c14;
	((uint8_t *)&result)[15] = c15;

	return result;
}

inline __m128i _mm_srli_si128_emu(__m128i a, int imm8)
{
	unsigned char result[16];
	uint8_t shift = imm8 & 0xff;
	if (shift > 15) shift = 16;

	int i;
	for (i = 0; i < (16 - shift); i++)
	{
		result[i] = ((unsigned char *)&a)[shift + i];
	}
	for (; i < 16; i++)
	{
		result[i] = 0;
	}

	return *(__m128i *)result;
}

inline __m128i _mm_xor_si128_emu(__m128i a, __m128i b)
{
#ifdef _WIN32
	uint64_t result[2];
	result[0] = *(uint64_t *)&a ^ *(uint64_t *)&b;
	result[1] = *((uint64_t *)&a + 1) ^ *((uint64_t *)&b + 1);
	return *(__m128i *)result;
#else
	return a ^ b;
#endif
}

inline __m128i _mm_load_si128_emu(const void *p)
{
	return *(__m128i *)p;
}

inline void _mm_store_si128_emu(void *p, __m128i val)
{
	*(__m128i *)p = val;
}

__m128i _mm_shuffle_epi8_emu(__m128i a, __m128i b)
{
	__m128i result;
	for (int i = 0; i < 16; i++)
	{
		if (((uint8_t *)&b)[i] & 0x80)
		{
			((uint8_t *)&result)[i] = 0;
		}
		else
		{
			((uint8_t *)&result)[i] = ((uint8_t *)&a)[((uint8_t *)&b)[i] & 0xf];
		}
	}


	return result;
}

// portable
static inline __m128i lazyLengthHash_port(uint64_t keylength, uint64_t length) {
	const __m128i lengthvector = _mm_set_epi64x_emu(keylength, length);
	const __m128i clprod1 = _mm_clmulepi64_si128_emu(lengthvector, lengthvector, 0x10);
	return clprod1;
}

// modulo reduction to 64-bit value. The high 64 bits contain garbage, see precompReduction64
static inline __m128i precompReduction64_si128_port(__m128i A) {

	//const __m128i C = _mm_set_epi64x(1U,(1U<<4)+(1U<<3)+(1U<<1)+(1U<<0)); // C is the irreducible poly. (64,4,3,1,0)
	const __m128i C = _mm_cvtsi64_si128_emu((1U << 4) + (1U << 3) + (1U << 1) + (1U << 0));
	__m128i Q2 = _mm_clmulepi64_si128_emu(A, C, 0x01);
	__m128i Q3 = _mm_shuffle_epi8_emu(_mm_setr_epi8_emu(0, 27, 54, 45, 108, 119, 90, 65, (char)216, (char)195, (char)238, (char)245, (char)180, (char)175, (char)130, (char)153),
		_mm_srli_si128_emu(Q2, 8));
	__m128i Q4 = _mm_xor_si128_emu(Q2, A);
	const __m128i final = _mm_xor_si128_emu(Q3, Q4);
	return final;/// WARNING: HIGH 64 BITS SHOULD BE ASSUMED TO CONTAIN GARBAGE
}

static inline uint64_t precompReduction64_port(__m128i A) {
	__m128i tmp = precompReduction64_si128_port(A);
	return _mm_cvtsi128_si64_emu(tmp);
}

// verus intermediate hash extra
static __m128i __verusclmulwithoutreduction64alignedrepeat_port(__m128i *randomsource, const __m128i buf[4], uint64_t keyMask)
{
	__m128i const *pbuf;

	keyMask >>= 4;

	__m128i acc = _mm_load_si128_emu(randomsource + (keyMask + 2));

	for (int64_t i = 0; i < 32; i++)
	{
		//std::cout << "LOOP " << i << " acc: " << LEToHex(acc) << std::endl;

		const uint64_t selector = _mm_cvtsi128_si64_emu(acc);

		// get two random locations in the key, which will be mutated and swapped
		__m128i *prand = randomsource + ((selector >> 5) & keyMask);
		__m128i *prandex = randomsource + ((selector >> 32) & keyMask);



		// select random start and order of pbuf processing
		pbuf = buf + (selector & 3);

		switch (selector & 0x1c)
		{
		case 0:
		{
			const __m128i temp1 = _mm_load_si128_emu(prandex);
			const __m128i temp2 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
			const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
			const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
			acc = _mm_xor_si128_emu(clprod1, acc);

			const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
			const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

			const __m128i temp12 = _mm_load_si128_emu(prand);
			_mm_store_si128_emu(prand, tempa2);

			const __m128i temp22 = _mm_load_si128_emu(pbuf);
			const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
			const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
			acc = _mm_xor_si128_emu(clprod12, acc);

			const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
			const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
			_mm_store_si128_emu(prandex, tempb2);
			break;
		}
		case 4:
		{
			const __m128i temp1 = _mm_load_si128_emu(prand);
			const __m128i temp2 = _mm_load_si128_emu(pbuf);
			const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
			const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
			acc = _mm_xor_si128_emu(clprod1, acc);
			const __m128i clprod2 = _mm_clmulepi64_si128_emu(temp2, temp2, 0x10);
			acc = _mm_xor_si128_emu(clprod2, acc);

			const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
			const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

			const __m128i temp12 = _mm_load_si128_emu(prandex);
			_mm_store_si128_emu(prandex, tempa2);

			const __m128i temp22 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
			const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
			acc = _mm_xor_si128_emu(add12, acc);

			const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
			const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
			_mm_store_si128_emu(prand, tempb2);
			break;
		}
		case 8:
		{
			const __m128i temp1 = _mm_load_si128_emu(prandex);
			const __m128i temp2 = _mm_load_si128_emu(pbuf);
			const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
			acc = _mm_xor_si128_emu(add1, acc);

			const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
			const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

			const __m128i temp12 = _mm_load_si128_emu(prand);
			_mm_store_si128_emu(prand, tempa2);

			const __m128i temp22 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
			const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
			const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
			acc = _mm_xor_si128_emu(clprod12, acc);
			const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
			acc = _mm_xor_si128_emu(clprod22, acc);

			const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
			const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
			_mm_store_si128_emu(prandex, tempb2);
			break;
		}
		case 0xc:
		{
			const __m128i temp1 = _mm_load_si128_emu(prand);
			const __m128i temp2 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
			const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);

			// cannot be zero here
			const int32_t divisor = (uint32_t)selector;

			acc = _mm_xor_si128_emu(add1, acc);

			const int64_t dividend = _mm_cvtsi128_si64_emu(acc);
			const __m128i modulo = _mm_cvtsi32_si128_emu(dividend % divisor);
			acc = _mm_xor_si128_emu(modulo, acc);

			const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
			const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

			if (dividend & 1)
			{
				const __m128i temp12 = _mm_load_si128_emu(prandex);
				_mm_store_si128_emu(prandex, tempa2);

				const __m128i temp22 = _mm_load_si128_emu(pbuf);
				const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
				const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
				acc = _mm_xor_si128_emu(clprod12, acc);
				const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
				acc = _mm_xor_si128_emu(clprod22, acc);

				const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
				const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
				_mm_store_si128_emu(prand, tempb2);
			}
			else
			{
				const __m128i tempb3 = _mm_load_si128_emu(prandex);
				_mm_store_si128_emu(prandex, tempa2);
				_mm_store_si128_emu(prand, tempb3);
			}
			break;
		}
		case 0x10:
		{
			// a few AES operations
			const __m128i *rc = prand;
			__m128i tmp;

			__m128i temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
			__m128i temp2 = _mm_load_si128_emu(pbuf);

			AES2_EMU(temp1, temp2, 0);
			MIX2_EMU(temp1, temp2);

			AES2_EMU(temp1, temp2, 4);
			MIX2_EMU(temp1, temp2);

			AES2_EMU(temp1, temp2, 8);
			MIX2_EMU(temp1, temp2);

			acc = _mm_xor_si128_emu(temp1, acc);
			acc = _mm_xor_si128_emu(temp2, acc);

			const __m128i tempa1 = _mm_load_si128_emu(prand);
			const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
			const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

			const __m128i tempa4 = _mm_load_si128_emu(prandex);
			_mm_store_si128_emu(prandex, tempa3);
			_mm_store_si128_emu(prand, tempa4);
			break;
		}
		case 0x14:
		{
			// we'll just call this one the monkins loop, inspired by Chris
			const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
			__m128i tmp; // used by MIX2

			uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
			__m128i *rc = prand;
			uint64_t aesround = 0;
			__m128i onekey;

			do
			{
				//std::cout << "acc: " << LEToHex(acc) << ", round check: " << LEToHex((selector & (0x10000000 << rounds))) << std::endl;

				// note that due to compiler and CPUs, we expect this to do:
				// if (selector & ((0x10000000 << rounds) & 0xffffffff) if rounds != 3 else selector & 0xffffffff80000000):
				if (selector & (0x10000000 << rounds))
				{
					onekey = _mm_load_si128_emu(rc++);
					const __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
					const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
					const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
					acc = _mm_xor_si128_emu(clprod1, acc);
				}
				else
				{
					onekey = _mm_load_si128_emu(rc++);
					__m128i temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
					const uint64_t roundidx = aesround++ << 2;
					AES2_EMU(onekey, temp2, roundidx);

					MIX2_EMU(onekey, temp2);

					acc = _mm_xor_si128_emu(onekey, acc);
					acc = _mm_xor_si128_emu(temp2, acc);
				}
			} while (rounds--);

			const __m128i tempa1 = _mm_load_si128_emu(prand);
			const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
			const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

			const __m128i tempa4 = _mm_load_si128_emu(prandex);
			_mm_store_si128_emu(prandex, tempa3);
			_mm_store_si128_emu(prand, tempa4);
			break;
		}
		case 0x18:
		{
			const __m128i temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
			const __m128i temp2 = _mm_load_si128_emu(prand);
			const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
			const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
			acc = _mm_xor_si128_emu(clprod1, acc);

			const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp2);
			const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp2);

			const __m128i tempb3 = _mm_load_si128_emu(prandex);
			_mm_store_si128_emu(prandex, tempa2);
			_mm_store_si128_emu(prand, tempb3);
			break;
		}
		case 0x1c:
		{
			const __m128i temp1 = _mm_load_si128_emu(pbuf);
			const __m128i temp2 = _mm_load_si128_emu(prandex);
			const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
			const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
			acc = _mm_xor_si128_emu(clprod1, acc);

			const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp2);
			const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp2);

			const __m128i tempa3 = _mm_load_si128_emu(prand);
			_mm_store_si128_emu(prand, tempa2);

			acc = _mm_xor_si128_emu(tempa3, acc);

			const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, tempa3);
			const __m128i tempb2 = _mm_xor_si128_emu(tempb1, tempa3);
			_mm_store_si128_emu(prandex, tempb2);
			break;
		}
		}
	}
	return acc;
}



void GenNewCLKey(unsigned char *seedBytes32, u128 *keyback)
{
	// generate a new key by chain hashing with Haraka256 from the last curbuf
	int n256blks = VERUS_KEY_SIZE >> 5;  //8832 >> 5
	int nbytesExtra = VERUS_KEY_SIZE & 0x1f;  //8832 & 0x1f
	unsigned char *pkey = (unsigned char*)keyback;
	unsigned char *psrc = seedBytes32;
	for (int i = 0; i < n256blks; i++)
	{
		haraka256_port(pkey, psrc);

		psrc = pkey;
		pkey += 32;
	}
	if (nbytesExtra)
	{
		unsigned char buf[32];
		haraka256_port(buf, psrc);
		memcpy(pkey, buf, nbytesExtra);
	}
}


uint64_t verusclhash_port(void * random, const unsigned char buf[64], uint64_t keyMask) {
	const unsigned int  m = 128;// we process the data in chunks of 16 cache lines
	__m128i * rs64 = (__m128i *)random;
	const __m128i * string = (const __m128i *) buf;

	__m128i  acc = __verusclmulwithoutreduction64alignedrepeat_port(rs64, string, keyMask);
	acc = _mm_xor_si128_emu(acc, lazyLengthHash_port(1024, 64));
	return precompReduction64_port(acc);
}






/*
** Write ZCASH_SOL_LEN bytes representing the encoded solution as per the
** Zcash protocol specs (512 x 21-bit inputs).
**
** out		ZCASH_SOL_LEN-byte buffer where the solution will be stored
** inputs	array of 32-bit inputs
** n		number of elements in array
*/
void store_encoded_sol(uint8_t *out, uint32_t *inputs, uint32_t n)
{
	uint32_t byte_pos = 0;
	int32_t bits_left = PREFIX + 1;
	uint8_t x = 0;
	uint8_t x_bits_used = 0;
	while (byte_pos < n)
	{
		if (bits_left >= 8 - x_bits_used)
		{
			x |= inputs[byte_pos] >> (bits_left - 8 + x_bits_used);
			bits_left -= 8 - x_bits_used;
			x_bits_used = 8;
		}
		else if (bits_left > 0)
		{
			uint32_t mask = ~(-1 << (8 - x_bits_used));
			mask = ((~mask) >> bits_left) & mask;
			x |= (inputs[byte_pos] << (8 - x_bits_used - bits_left)) & mask;
			x_bits_used += bits_left;
			bits_left = 0;
		}
		else if (bits_left <= 0)
		{
			assert(!bits_left);
			byte_pos++;
			bits_left = PREFIX + 1;
		}
		if (x_bits_used == 8)
		{
			*out++ = x;
			x = x_bits_used = 0;
		}
	}
}

/*
** Print on stdout a hex representation of the encoded solution as per the
** zcash protocol specs (512 x 21-bit inputs).
**
** inputs	array of 32-bit inputs
** n		number of elements in array
*/
void print_encoded_sol(uint32_t *inputs, uint32_t n)
{
	uint8_t	sol[ZCASH_SOL_LEN];
	uint32_t	i;
	store_encoded_sol(sol, inputs, n);
	for (i = 0; i < sizeof(sol); i++)
		printf("%02x", sol[i]);
	printf("\n");
	fflush(stdout);
}

void print_sol(uint32_t *values, uint64_t *nonce)
{
	uint32_t	show_n_sols;
	show_n_sols = (1 << PARAM_K);
	if (verbose < 2)
		show_n_sols = MIN(10, show_n_sols);
	fprintf(stderr, "Soln:");
	// for brievity, only print "small" nonces
	if (*nonce < (1ULL << 32))
		fprintf(stderr, " 0x%" PRIx64 ":", *nonce);
	for (unsigned i = 0; i < show_n_sols; i++)
		fprintf(stderr, " %x", values[i]);
	fprintf(stderr, "%s\n", (show_n_sols != (1 << PARAM_K) ? "..." : ""));
}

/*
** Compare two 256-bit values interpreted as little-endian 256-bit integers.
*/
int32_t cmp_target_256(void *_a, void *_b)
{
	uint8_t	*a = _a;
	uint8_t	*b = _b;
	int32_t	i;
	for (i = SHA256_TARGET_LEN - 1; i >= 0; i--)
		if (a[i] != b[i])
			return (int32_t)a[i] - b[i];
	return 0;
}

void Verus2hash(unsigned char *hash, unsigned char *curBuf, uint32_t nonce)
{
	uint64_t mask = VERUS_KEY_SIZE128; //552

									   //	GenNewCLKey(curBuf, data_key[thr_id]);  //data_key a global static 2D array data_key[16][8832];
	((uint32_t*)&curBuf[0])[8] = nonce;
	uint64_t intermediate = verusclhash_port(data_key, curBuf, VERUS_KEY_SIZE);
	//FillExtra
	memcpy(curBuf + 47, &intermediate, 8);
	memcpy(curBuf + 55, &intermediate, 8);
	memcpy(curBuf + 63, &intermediate, 1);

	haraka512_port_keyed(hash, curBuf, data_key + (intermediate & mask));
}

/*
** Verify if the solution's block hash is under the target, and if yes print
** it formatted as:
** "sol: <job_id> <ntime> <nonce_rightpart> <solSize+sol>"
**
** Return 1 iff the block hash is under the target.
*/
uint32_t print_solver_line(uint32_t *values, uint8_t *header,
	size_t fixed_nonce_bytes, uint8_t *target, char *job_id)
{
	uint8_t	buffer[ZCASH_BLOCK_HEADER_LEN + ZCASH_SOLSIZE_LEN +
		ZCASH_SOL_LEN];
	uint8_t	hash0[SHA256_DIGEST_SIZE];
	uint8_t	hash1[SHA256_DIGEST_SIZE];
	uint8_t	*p;
	p = buffer;
	memcpy(p, header, ZCASH_BLOCK_HEADER_LEN);
	p += ZCASH_BLOCK_HEADER_LEN;
	memcpy(p, "\xfd\x40\x05", ZCASH_SOLSIZE_LEN);
	p += ZCASH_SOLSIZE_LEN;
	store_encoded_sol(p, values, 1 << PARAM_K);
	Sha256_Onestep(buffer, sizeof(buffer), hash0);
	Sha256_Onestep(hash0, sizeof(hash0), hash1);
	// compare the double SHA256 hash with the target
	if (cmp_target_256(target, hash1) < 0)
	{
		debug("Hash is above target\n");
		return 0;
	}
	debug("Hash is under target\n");
	printf("sol: %s ", job_id);
	p = header + ZCASH_BLOCK_OFFSET_NTIME;
	printf("%02x%02x%02x%02x ", p[0], p[1], p[2], p[3]);
	printf("%s ", s_hexdump(header + ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN +
		fixed_nonce_bytes, ZCASH_NONCE_LEN - fixed_nonce_bytes));
	printf("%s%s\n", ZCASH_SOLSIZE_HEX,
		s_hexdump(buffer + ZCASH_BLOCK_HEADER_LEN + ZCASH_SOLSIZE_LEN,
			ZCASH_SOL_LEN));
	fflush(stdout);
	return 1;
}

int sol_cmp(const void *_a, const void *_b)
{
	const uint32_t	*a = _a;
	const uint32_t	*b = _b;
	for (uint32_t i = 0; i < (1 << PARAM_K); i++)
	{
		if (*a != *b)
			return *a - *b;
		a++;
		b++;
	}
	return 0;
}

/*
** Print all solutions.
**
** In mining mode, return the number of shares, that is the number of solutions
** that were under the target.
*/
uint32_t print_sols(sols_t *all_sols, uint64_t *nonce, uint32_t nr_valid_sols,
	uint8_t *header, size_t fixed_nonce_bytes, uint8_t *target,
	char *job_id)
{
	uint8_t		*valid_sols;
	uint32_t		counted;
	uint32_t		shares = 0;
	valid_sols = malloc(nr_valid_sols * SOL_SIZE);
	if (!valid_sols)
		fatal("malloc: %s\n", strerror(errno));
	counted = 0;
	for (uint32_t i = 0; i < all_sols->nr; i++)
		if (all_sols->valid[i])
		{
			if (counted >= nr_valid_sols)
				fatal("Bug: more than %d solutions\n", nr_valid_sols);
			memcpy(valid_sols + counted * SOL_SIZE, all_sols->values[i],
				SOL_SIZE);
			counted++;
		}
	assert(counted == nr_valid_sols);
	// sort the solutions amongst each other, to make the solver's output
	// deterministic and testable
	qsort(valid_sols, nr_valid_sols, SOL_SIZE, sol_cmp);
	for (uint32_t i = 0; i < nr_valid_sols; i++)
	{
		uint32_t	*inputs = (uint32_t *)(valid_sols + i * SOL_SIZE);
		if (!mining && show_encoded)
			print_encoded_sol(inputs, 1 << PARAM_K);
		if (verbose)
			print_sol(inputs, nonce);
		if (mining)
			shares += print_solver_line(inputs, header, fixed_nonce_bytes,
				target, job_id);
	}
	free(valid_sols);
	return shares;
}

/*
** Sort a pair of binary blobs (a, b) which are consecutive in memory and
** occupy a total of 2*len 32-bit words.
**
** a            points to the pair
** len          number of 32-bit words in each pair
*/
void sort_pair(uint32_t *a, uint32_t len)
{
	uint32_t    *b = a + len;
	uint32_t     tmp, need_sorting = 0;
	for (uint32_t i = 0; i < len; i++)
		if (need_sorting || a[i] > b[i])
		{
			need_sorting = 1;
			tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		}
		else if (a[i] < b[i])
			return;
}

/*
** If solution is invalid return 0. If solution is valid, sort the inputs
** and return 1.
*/

#define SEEN_LEN (1 << (PREFIX + 1)) / 8

uint32_t verify_sol(sols_t *sols, unsigned sol_i)
{
	uint32_t	*inputs = sols->values[sol_i];
	//uint32_t	seen_len = (1 << (PREFIX + 1)) / 8; 
	//uint8_t	seen[seen_len]; // @mrb MSVC didn't like this.
	uint8_t	seen[SEEN_LEN];
	uint32_t	i;
	uint8_t	tmp;
	// look for duplicate inputs
	memset(seen, 0, SEEN_LEN);
	for (i = 0; i < (1 << PARAM_K); i++)
	{
		if (inputs[i] / 8 >= SEEN_LEN)
		{
			warn("Invalid input retrieved from device: %d\n", inputs[i]);
			sols->valid[sol_i] = 0;
			return 0;
		}
		tmp = seen[inputs[i] / 8];
		seen[inputs[i] / 8] |= 1 << (inputs[i] & 7);
		if (tmp == seen[inputs[i] / 8])
		{
			// at least one input value is a duplicate
			sols->valid[sol_i] = 0;
			return 0;
		}
	}
	// the valid flag is already set by the GPU, but set it again because
	// I plan to change the GPU code to not set it
	sols->valid[sol_i] = 1;
	// sort the pairs in place
	for (uint32_t level = 0; level < PARAM_K; level++)
		for (i = 0; i < (1 << PARAM_K); i += (2 << level))
			sort_pair(&inputs[i], 1 << level);
	return 1;
}
int fulltest(const uint32_t *hash, const uint32_t *target)
{
	int i;
	int rc = 1;

	for (i = 7; i >= 0; i--) {
		if (hash[i] > target[i]) {
			rc = 0;
			break;
		}
		if (hash[i] < target[i]) {
			rc = 1;
			break;
		}
		if (hash[1] == target[1]) {

		}
	}

	return rc;
}
/*
** Return the number of valid solutions.
*/
uint32_t verify_nonce(cl_command_queue queue, cl_mem nonces_d,
	uint8_t *header, size_t fixed_nonce_bytes, uint8_t *target,
	char *job_id, uint32_t *shares, uchar *verus, uint32_t **pnonce)
{
	uint8_t	buffer[1347] = { 0 };
	unsigned char	*p;
	uint32_t	sh = 0;

	uint32_t    winning_n;


	check_clEnqueueReadBuffer(queue, nonces_d,
		CL_TRUE,	// cl_bool	blocking_read
		0,		// size_t	offset
		sizeof(uint32_t) * 1,	// size_t	size
		pnonce,	// void		*ptr
		0,		// cl_uint	num_events_in_wait_list
		NULL,	// cl_event	*event_wait_list
		NULL);	// cl_event	*event

	winning_n = pnonce[0];

	if (winning_n != 0xfffffffful)

	{
		uint32_t vhash[8];
		//	Verus2hash((unsigned char *)vhash, (unsigned char *)verus, winning_n);
		//	printf("GPU hash end= %08x\n", vhash[7]);
		const uint32_t Htarg = ((uint32_t*)&target[0])[7];

		if (vhash[7] >= Htarg || fulltest(vhash, (uint32_t*)target)) {
			((uint32_t*)&buffer)[333] = winning_n & 0xffffffff;
			//	((uint32_t*)&buffer)[334] = winning_n >> 32;
			buffer[0] = 0xfd; buffer[1] = 0x40; buffer[2] = 0x05; buffer[3] = 0x03;
			sh = 1;

			debug("Hash is under target\n");
			printf("sol: %s ", job_id);
			p = header + ZCASH_BLOCK_OFFSET_NTIME;
			printf("%02x%02x%02x%02x ", p[0], p[1], p[2], p[3]);
			printf("%s ", s_hexdump(header + ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN +
				fixed_nonce_bytes, ZCASH_NONCE_LEN - fixed_nonce_bytes));
			printf("%s\n", s_hexdump(buffer, 1347));
			fflush(stdout);
		}

	}

	//	if (shares)
	//	*shares = sh;  //***NOT SURE MAY HAVE TO CHECK ***

	return sh;
}

unsigned get_value(unsigned *data, unsigned row)
{
	return data[row];
}




void VerusHashHalf(uint8_t *result2, uint8_t *data, int len)
{
	unsigned char buf1[64] = { 0 }, buf2[64] = { 0 };
	unsigned char *curBuf = buf1, *result = buf2;
	size_t curPos = 0;
	//unsigned char result[64];
	curBuf = buf1;
	result = buf2;
	curPos = 0;
	memset(buf1, 64, 0);

	unsigned char *tmp;

	load_constants_port();

	// digest up to 32 bytes at a time
	for (int pos = 0; pos < len; )
	{
		int room = 32 - curPos;

		if (len - pos >= room)
		{
			memcpy(curBuf + 32 + curPos, data + pos, room);

			haraka512_port(result, curBuf);


			tmp = curBuf;
			curBuf = result;
			result = tmp;
			pos += room;
			curPos = 0;
		}
		else
		{
			memcpy(curBuf + 32 + curPos, data + pos, len - pos);
			curPos += len - pos;
			pos = len;
		}
	}

	memcpy(curBuf + 47, curBuf, 16);
	memcpy(curBuf + 63, curBuf, 1);
	//	FillExtra((u128 *)curBuf);
	memcpy(result2, curBuf, 64);
};


int read_last_line(char *buf, size_t len, int block)
{
	char	*start;
	size_t	pos = 0;
	ssize_t	n;
#ifndef WIN32
	set_blocking_mode(0, block);
#endif
	while (42)
	{
#ifndef WIN32
		n = read(0, buf + pos, len - pos);
		if (n == -1 && errno == EINTR)
			continue;
		else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			if (!pos)
				return 0;
			warn("strange: a partial line was read\n");
			// a partial line was read, continue reading it in blocking mode
			// to be sure to read it completely
			set_blocking_mode(0, 1);
			continue;
		}
		else if (n == -1)
			fatal("read stdin: %s\n", strerror(errno));
		else if (!n)
			fatal("EOF on stdin\n");
		pos += n;

		if (buf[pos - 1] == '\n')
			// 1 (or more) complete lines were read
			break;
#else
		DWORD bytesAvailable = 0;
		HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
		PeekNamedPipe(stdinHandle, NULL, 0, NULL, &bytesAvailable, NULL);

		if (bytesAvailable > 0) {

			if (!ReadFile(stdinHandle, buf, bytesAvailable, &bytesAvailable, NULL)) {

				fatal("ReadFile: %d", GetLastError());
			}
			pos += bytesAvailable;
		}
		else {
			return 0;
		}
		if (buf[pos - 1] == '\n')
			// 1 (or more) complete lines were read
			break;
#endif
	}
	start = memrchr(buf, '\n', pos - 1);
	if (start)
	{
		warn("strange: more than 1 line was read\n");
		// more than 1 line; copy the last line to the beginning of the buffer
		pos -= (start + 1 - buf);
		memmove(buf, start + 1, pos);
	}
	// overwrite '\n' with NUL

	buf[pos - 1] = 0;
	return 1;
}

/*
** Parse a string:
**   "<target> <job_id> <header> <nonce_leftpart>"
** (all the parts are in hex, except job_id which is a non-whitespace string),
** decode the hex values and store them in the relevant buffers.
**
** The remaining part of <header> that is not set by
** <header><nonce_leftpart> will be randomized so that the miner
** solves a unique Equihash PoW.
**
** str		string to parse
** target	buffer where the <target> will be stored
** target_len	size of target buffer
** job_id	buffer where the <job_id> will be stored
** job_id_len	size of job_id buffer
** header	buffer where the <header><nonce_leftpart> will be
** 		concatenated and stored
** header_len	size of the header_buffer
** fixed_nonce_bytes
** 		nr of bytes represented by <nonce_leftpart> will be stored here;
** 		this is the number of nonce bytes fixed by the stratum server
*/
void mining_parse_job(char *str, uint8_t *target, size_t target_len,
	char *job_id, size_t job_id_len, uint8_t *header, size_t header_len,
	size_t *fixed_nonce_bytes)
{
	uint32_t		str_i, i;
	// parse target
	str_i = 0;
	for (i = 0; i < target_len; i++, str_i += 2)
		target[i] = hex2val(str, str_i) * 16 + hex2val(str, str_i + 1);
	assert(str[str_i] == ' ');
	str_i++;
	// parse job_id
	for (i = 0; i < job_id_len && str[str_i] != ' '; i++, str_i++)
		job_id[i] = str[str_i];
	assert(str[str_i] == ' ');
	assert(i < job_id_len);
	job_id[i] = 0;
	str_i++;
	// parse header and nonce_leftpart
	for (i = 0; i < header_len && str[str_i] != ' '; i++, str_i += 2)
		header[i] = hex2val(str, str_i) * 16 + hex2val(str, str_i + 1);
	assert(str[str_i] == ' ');
	str_i++;
	*fixed_nonce_bytes = 0;
	while (i < header_len && str[str_i] && str[str_i] != '\n')
	{
		header[i] = hex2val(str, str_i) * 16 + hex2val(str, str_i + 1);
		i++;
		str_i += 2;
		(*fixed_nonce_bytes)++;
	}
	assert(!str[str_i]);
	// Randomize rest of the bytes except N_ZERO_BYTES bytes which must be zero
	debug("Randomizing %d bytes in nonce\n", header_len - N_ZERO_BYTES - i);
	randomize(header + i, header_len - N_ZERO_BYTES - i);
	memset(header + header_len - N_ZERO_BYTES, 0, N_ZERO_BYTES);
}

/*
** Run in mining mode.
*/
#ifdef WIN32

#ifndef DEFAULT_NUM_MINING_MODE_THREADS
#define DEFAULT_NUM_MINING_MODE_THREADS 1
#define MAX_NUM_MINING_MODE_THREADS 16
#endif
uint32_t num_mining_mode_threads = DEFAULT_NUM_MINING_MODE_THREADS;
CRITICAL_SECTION cs;

struct mining_mode_thread_args {
	cl_device_id dev_id;
	cl_context ctx;
	cl_command_queue queue;

	//
	uint8_t     header[ZCASH_BLOCK_HEADER_LEN];
	uint8_t		target[SHA256_DIGEST_SIZE];
	char		job_id[256];
	size_t		fixed_nonce_bytes;
	uint64_t		*total;
	uint64_t		*total_shares;
};

#define ARGS ((struct mining_mode_thread_args *)args)


void mining_mode(cl_device_id dev_id, cl_program program, cl_context ctx, cl_command_queue queue,
	cl_kernel *k_verus, uint8_t *header)
{

	char		line[4096];
	uint8_t		target[SHA256_DIGEST_SIZE];
	char		job_id[256];
	size_t		fixed_nonce_bytes = 0;
	uint64_t		i;
	uint64_t		total = 0;
	uint32_t		shares;
	uint64_t		total_shares = 0;
	uint64_t		t0 = 0, t1;
	uint64_t		status_period = 300e3; // time (usec) between statuses
	cl_int          status;
	uint8_t         *blockhash_half; // [64] = { 0 };
	uint8_t         *ptarget;
	uint32_t		*pnonces;
	cl_mem          key_const_d, data_keylarge_d, blockhash_half_d, target_d, resnonces_d, startNonce_d, fix_rand, fix_randex, acc_d;
	size_t		    global_ws;
	size_t          local_work_size = 256;
	uint32_t		nonces_total = 0;
	uint64_t		*nonce_ptr;
	uint32_t        *nonce_sum;

	unsigned char block_41970[] = { 0xfd, 0x40, 0x05, 0x03 };

	uint8_t full_data[140 + 3 + 1344] = { 0 };
	uint8_t* sol_data = &full_data[140];

	blockhash_half = (uint8_t*)malloc(sizeof(uint8_t) * 64);
	ptarget = (uint8_t *)malloc(sizeof(uint8_t) * 32);
	pnonces = (uint32_t *)malloc(sizeof(uint32_t) * 1);
	nonce_sum = (uint32_t *)malloc(sizeof(uint32_t) * 1);
	data_key = (u128 *)malloc(VERUS_KEY_SIZE);
	uint32_t num_sols;

	key_const_d = check_clCreateBuffer(ctx, CL_MEM_READ_ONLY, sizeof(uint8_t) * VERUS_KEY_SIZE, NULL);
	blockhash_half_d = check_clCreateBuffer(ctx, CL_MEM_READ_ONLY, sizeof(uint8_t) * 64, NULL);
	target_d = check_clCreateBuffer(ctx, CL_MEM_READ_ONLY, sizeof(uint8_t) * 32, NULL);
	resnonces_d = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint32_t) * 2, NULL);
	startNonce_d = check_clCreateBuffer(ctx, CL_MEM_READ_ONLY, sizeof(uint32_t) * 2, NULL);
	data_keylarge_d = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint8_t) * VERUS_KEY_SIZE * VERUS_WORKSIZE, NULL);
	fix_rand = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint32_t) * VERUS_WORKSIZE * 32, NULL);
	fix_randex = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint32_t) * VERUS_WORKSIZE * 32, NULL);
	nonce_ptr = (uint64_t *)(header + ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN);
	acc_d = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint64_t) * VERUS_WORKSIZE, NULL);
	InitializeCriticalSection(&cs);

	puts("SILENTARMY mining mode ready");
	fflush(stdout);
	SetConsoleOutputCP(65001);
	int started = 0;
	while (1)
	{
		int changed = 0;
		// iteration #0 always reads a job or else there is nothing to do

		nonce_sum[0] = 0;
		if (read_last_line(line, sizeof(line), !i)) {
			changed = 1; started = 1;
			EnterCriticalSection(&cs);
			mining_parse_job(line,
				target, sizeof(target),
				job_id, sizeof(job_id),
				header, ZCASH_BLOCK_HEADER_LEN,
				&fixed_nonce_bytes);
			LeaveCriticalSection(&cs);

			memcpy(full_data, header, 140);
			memcpy(sol_data, block_41970, 4);
			//memcpy(full_data, data, 1487);

			VerusHashHalf(blockhash_half, (unsigned char*)full_data, 1487);
			GenNewCLKey((unsigned char*)blockhash_half, data_key);
			for (int j = 0; j < 32; j++)
				ptarget[j] = target[j];

			pnonces[0] = 0xfffffffful;
			nonce_sum[0] = 0x0ul;
			// send header,key,target to GPU verus_setBlock(blockhash_half, target, (uint8_t*)data_key, throughput); //set data to gpu kernel

		}
		else if (nonce_sum[0] == 0)  //main nonce needs incrementing
		{
			changed = 1;
			// increment bytes 17-19
			(*(uint32_t *)((uint8_t *)nonce_ptr + 17))++;
			// byte 20 and above must be zero
			*(uint32_t *)((uint8_t *)nonce_ptr + 20) = 0;

			memcpy(full_data, header, 140);
			memcpy(sol_data, block_41970, 4);
			//memcpy(full_data, header, 1487);

			VerusHashHalf(blockhash_half, (unsigned char*)full_data, 1487);
			GenNewCLKey((unsigned char*)blockhash_half, data_key);

			for (int j = 0; j < 32; j++)
				ptarget[j] = target[j];

		}
		if (started) {



			if (changed == 1) {  // if the main key changes we have to reload the global mem

				status = clEnqueueWriteBuffer(queue, blockhash_half_d, CL_TRUE, 0, sizeof(uint8_t) * 64, blockhash_half, 0, NULL, NULL);
				if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);
				status = clEnqueueWriteBuffer(queue, target_d, CL_TRUE, 0, sizeof(uint8_t) * 32, ptarget, 0, NULL, NULL);
				if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);
				status = clEnqueueWriteBuffer(queue, key_const_d, CL_TRUE, 0, sizeof(uint8_t) * VERUS_KEY_SIZE, data_key, 0, NULL, NULL);
				if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);

				check_clSetKernelArg(k_verus[0], 0, &key_const_d);
				check_clSetKernelArg(k_verus[0], 1, &data_keylarge_d);


				global_ws = VERUS_WORKSIZE * 128;
				local_work_size = 128;

				check_clEnqueueNDRangeKernel(queue, k_verus[0], 1, NULL,
					&global_ws, &local_work_size, 0, NULL, NULL);
				//	clFinish(queue);

			}

			pnonces[0] = 0xfffffffful;
			status = clEnqueueWriteBuffer(queue, resnonces_d, CL_TRUE, 0, sizeof(uint32_t) * 1, pnonces, 0, NULL, NULL);
			if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);
			status = clEnqueueWriteBuffer(queue, startNonce_d, CL_TRUE, 0, sizeof(uint32_t) * 1, nonce_sum, 0, NULL, NULL);
			if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);


			check_clSetKernelArg(k_verus[1], 0, &startNonce_d);
			check_clSetKernelArg(k_verus[1], 1, &blockhash_half_d);
			check_clSetKernelArg(k_verus[1], 2, &data_keylarge_d);
			check_clSetKernelArg(k_verus[1], 3, &acc_d);
			check_clSetKernelArg(k_verus[1], 4, &fix_rand);
			check_clSetKernelArg(k_verus[1], 5, &fix_randex);

			global_ws = VERUS_WORKSIZE;
			local_work_size = MAIN_THREADS;

			check_clEnqueueNDRangeKernel(queue, k_verus[1], 1, NULL,
				&global_ws, &local_work_size, 0, NULL, NULL);

			//	clFinish(queue);  //maybe remove???

			check_clSetKernelArg(k_verus[2], 0, &startNonce_d);
			check_clSetKernelArg(k_verus[2], 1, &blockhash_half_d);
			check_clSetKernelArg(k_verus[2], 2, &target_d);
			check_clSetKernelArg(k_verus[2], 3, &resnonces_d);
			check_clSetKernelArg(k_verus[2], 4, &data_keylarge_d);
			check_clSetKernelArg(k_verus[2], 5, &acc_d);


			global_ws = VERUS_WORKSIZE;
			local_work_size = 256;

			check_clEnqueueNDRangeKernel(queue, k_verus[2], 1, NULL,
				&global_ws, &local_work_size, 0, NULL, NULL);

			//	clFinish(queue);

			num_sols = verify_nonce(queue, resnonces_d, header,
				fixed_nonce_bytes, target, job_id, shares, blockhash_half, pnonces);

			check_clSetKernelArg(k_verus[3], 0, &key_const_d);
			check_clSetKernelArg(k_verus[3], 1, &data_keylarge_d);
			check_clSetKernelArg(k_verus[3], 2, &fix_rand);
			check_clSetKernelArg(k_verus[3], 3, &fix_randex);

			global_ws = VERUS_WORKSIZE * 32;
			local_work_size = 32;

			check_clEnqueueNDRangeKernel(queue, k_verus[3], 1, NULL,
				&global_ws, &local_work_size, 0, NULL, NULL);

			//	clFinish(queue);

			total += VERUS_WORKSIZE;
			total_shares += num_sols;
			if ((nonce_sum[0] + VERUS_WORKSIZE) < 0xfffffffful)
				nonce_sum[0] += VERUS_WORKSIZE;
			else
				nonce_sum[0] = 0;


			if ((t1 = now()) > t0 + status_period)
			{

				EnterCriticalSection(&cs);
				t0 = t1;

				printf("status: %" PRId64 " %" PRId64 "\n", total / 1000000, total_shares);
				fflush(stdout);
				LeaveCriticalSection(&cs);
				//fprintf(stderr, " (%d kh/s)\n",
				//	(t3 - t2) / 1e3, total / ((t3 - t2) / 1e6) / 1000);
				//fflush(stdout);
			}
		}
	}



}
#else
void mining_mode(
	cl_device_id *dev_id,
	cl_program program,
	cl_context ctx,
	cl_command_queue queue,
	cl_kernel k_init_ht,
	cl_kernel *k_rounds,
	cl_kernel k_potential_sols,
	cl_kernel k_sols,
	cl_mem *buf_ht,
	cl_mem buf_potential_sols,
	cl_mem buf_sols,
	cl_mem buf_dbg,
	size_t dbg_size,
	uint8_t *header,
	cl_mem *rowCounters)
{
	char		line[4096];
	uint8_t		target[SHA256_DIGEST_SIZE];
	char		job_id[256];
	size_t		fixed_nonce_bytes = 0;
	uint64_t		i;
	uint64_t		total = 0;
	uint32_t		shares;
	uint64_t		total_shares = 0;
	uint64_t		t0 = 0, t1;
	uint64_t		status_period = 500e3; // time (usec) between statuses

	puts("SILENTARMY mining mode ready");
	fflush(stdout);
#ifdef WIN32
	SetConsoleOutputCP(65001);
#endif
	for (i = 0; ; i++)
	{
		// iteration #0 always reads a job or else there is nothing to do

		if (read_last_line(line, sizeof(line), !i)) {
			mining_parse_job(line,
				target, sizeof(target),
				job_id, sizeof(job_id),
				header, ZCASH_BLOCK_HEADER_LEN,
				&fixed_nonce_bytes);
		}
		total += solve_equihash(ctx, queue, k_init_ht, k_rounds, k_potential_sols, k_sols, buf_ht,
			buf_potential_sols, buf_sols, buf_dbg, dbg_size, header, ZCASH_BLOCK_HEADER_LEN, 1,
			fixed_nonce_bytes, target, job_id, &shares, rowCounters);
		total_shares += shares;
		if ((t1 = now()) > t0 + status_period)
		{
			t0 = t1;
			printf("status: %" PRId64 " %" PRId64 "\n", total, total_shares);
			fflush(stdout);
		}
	}
}
#endif

void run_opencl(uint8_t *header, size_t header_len, cl_device_id *dev_id, cl_context ctx,
	cl_command_queue queue, cl_program program, cl_kernel *k_verus)
{
	void                *dbg = NULL;
	uint64_t		nonce;
	uint64_t		total;

	mining_mode(*dev_id, program, ctx, queue, k_verus, header);

}

/*
** Scan the devices available on this platform. Try to find the device
** selected by the "--use <id>" option and, if found, store the platform and
** device in plat_id and dev_id.
**
** plat			platform being scanned
** nr_devs_total	total number of devices detected so far, will be
** 			incremented by the number of devices available on this
** 			platform
** plat_id		where to store the platform id
** dev_id		where to store the device id
**
** Return 1 iff the selected device was found.
*/
unsigned scan_platform(cl_platform_id plat, cl_uint *nr_devs_total,
	cl_platform_id *plat_id, cl_device_id *dev_id)
{
	cl_device_type	typ = CL_DEVICE_TYPE_ALL;
	cl_uint		nr_devs = 0;
	cl_device_id	*devices;
	cl_int		status;
	unsigned		found = 0;
	unsigned		i;
	if (do_list_devices)
		print_platform_info(plat);
	status = clGetDeviceIDs(plat, typ, 0, NULL, &nr_devs);
	if (status != CL_SUCCESS)
		fatal("clGetDeviceIDs (%d)\n", status);
	if (nr_devs == 0)
		return 0;
	devices = (cl_device_id *)malloc(nr_devs * sizeof(*devices));
	status = clGetDeviceIDs(plat, typ, nr_devs, devices, NULL);
	if (status != CL_SUCCESS)
		fatal("clGetDeviceIDs (%d)\n", status);
	i = 0;
	while (i < nr_devs)
	{
		if (do_list_devices)
			print_device_info(*nr_devs_total, devices[i]);
		else if (*nr_devs_total == gpu_to_use)
		{
			found = 1;
			*plat_id = plat;
			*dev_id = devices[i];
			break;
		}
		(*nr_devs_total)++;
		i++;
	}
	free(devices);
	return found;
}

/*
** Stores the platform id and device id that was selected by the "--use <id>"
** option.
**
** plat_id		where to store the platform id
** dev_id		where to store the device id
*/
void scan_platforms(cl_platform_id *plat_id, cl_device_id *dev_id)
{
	cl_uint		nr_platforms;
	cl_platform_id	*platforms;
	cl_uint		i, nr_devs_total;
	cl_int		status;
	status = clGetPlatformIDs(0, NULL, &nr_platforms);
	if (status != CL_SUCCESS)
		fatal("Cannot get OpenCL platforms (%d)\n", status);
	if (!nr_platforms || verbose)
		fprintf(stderr, "Found %d OpenCL platform(s)\n", nr_platforms);
	if (!nr_platforms)
		exit(1);
	platforms = (cl_platform_id *)malloc(nr_platforms * sizeof(*platforms));
	if (!platforms)
		fatal("malloc: %s\n", strerror(errno));
	status = clGetPlatformIDs(nr_platforms, platforms, NULL);
	if (status != CL_SUCCESS)
		fatal("clGetPlatformIDs (%d)\n", status);
	i = nr_devs_total = 0;
	while (i < nr_platforms)
	{
		if (scan_platform(platforms[i], &nr_devs_total, plat_id, dev_id))
			break;
		i++;
	}
	if (do_list_devices)
		exit(0);
	debug("Using GPU device ID %d\n", gpu_to_use);
	amd_flag = is_platform_amd(*plat_id);
	free(platforms);
}

void init_and_run_opencl(uint8_t *header, size_t header_len)
{
	cl_platform_id	plat_id = 0;
	cl_device_id	dev_id = 0;
	cl_kernel		k_verus[4];
	cl_int		status;
	scan_platforms(&plat_id, &dev_id);

	if (!plat_id || !dev_id)
		fatal("Selected device (ID %d) not found; see --list\n", gpu_to_use);
	/* Create context.*/
	cl_context context = clCreateContext(NULL, 1, &dev_id,
		NULL, NULL, &status);
	if (status != CL_SUCCESS || !context)
		fatal("clCreateContext (%d)\n", status);
	/* Creating command queue associate with the context.*/
	cl_command_queue queue = clCreateCommandQueue(context, dev_id,
		0, &status);
	if (status != CL_SUCCESS || !queue)
		fatal("clCreateCommandQueue (%d)\n", status);
	/* Create program object */
#ifdef WIN32
	load_file("input.cl", &source, &source_len, 0);
	//printf("%s", source);
#else
	source = ocl_code;
#endif
	source_len = strlen(source);
	cl_program program;

	program = clCreateProgramWithSource(context, 1, (const char **)&source,
		&source_len, &status);
	if (status != CL_SUCCESS || !program)
		fatal("clCreateProgramWithSource (%d)\n", status);
	/* Build program. */
	if (!mining || verbose)
		fprintf(stderr, "Building program\n");
	status = clBuildProgram(program, 1, &dev_id,
		"", // compile options
		NULL, NULL);
	if (status != CL_SUCCESS)
	{
		printf("OpenCL build failed (%d). Build log follows:\n", status);
		get_program_build_log(program, dev_id);
		fflush(stdout);
		exit(1);
	}
	get_program_bins(program);
	// Create kernel objects

	k_verus[0] = clCreateKernel(program, "verus_key", &status);
	if (status != CL_SUCCESS || !k_verus[0])
		fatal("clCreateKernel1 (%d)\n", status);
	k_verus[1] = clCreateKernel(program, "verus_gpu_hash", &status);
	if (status != CL_SUCCESS || !k_verus[1])
		fatal("clCreateKernel2 (%d)\n", status);
	k_verus[2] = clCreateKernel(program, "verus_gpu_final", &status);
	if (status != CL_SUCCESS || !k_verus[2])
		fatal("clCreateKernel3 (%d)\n", status);
	k_verus[3] = clCreateKernel(program, "verus_extra_gpu_fix", &status);
	if (status != CL_SUCCESS || !k_verus[3])
		fatal("clCreateKernel4 (%d)\n", status);


	// Run
	run_opencl(header, header_len, &dev_id, context, queue, program, k_verus);
	// Release resources
	assert(CL_SUCCESS == 0);
	status = CL_SUCCESS;
	status |= clReleaseKernel(k_verus);
	status |= clReleaseProgram(program);
	status |= clReleaseCommandQueue(queue);
	status |= clReleaseContext(context);
	if (status)
		fprintf(stderr, "Cleaning resources failed\n");
}

uint32_t parse_header(uint8_t *h, size_t h_len, const char *hex)
{
	size_t      hex_len;
	size_t      bin_len;
	size_t	opt0 = ZCASH_BLOCK_HEADER_LEN;
	size_t      i;
	if (!hex)
	{
		if (!do_list_devices && !mining)
			fprintf(stderr, "Solving default all-zero %zd-byte header\n", opt0);
		return opt0;
	}
	hex_len = strlen(hex);
	bin_len = hex_len / 2;
	if (hex_len % 2)
		fatal("Error: input header must be an even number of hex digits\n");
	if (bin_len != opt0)
		fatal("Error: input header must be a %zd-byte full header\n", opt0);
	assert(bin_len <= h_len);
	for (i = 0; i < bin_len; i++)
		h[i] = hex2val(hex, i * 2) * 16 + hex2val(hex, i * 2 + 1);
	while (--i >= bin_len - N_ZERO_BYTES)
		if (h[i])
			fatal("Error: last %d bytes of full header (ie. last %d "
				"bytes of 32-byte nonce) must be zero due to an "
				"optimization in my BLAKE2b implementation\n",
				N_ZERO_BYTES, N_ZERO_BYTES);
	return bin_len;
}

enum
{
	OPT_HELP,
	OPT_VERBOSE,
	OPT_INPUTHEADER,
	OPT_NONCES,
	OPT_THREADS,
	OPT_T,
	OPT_B,
	OPT_LIST,
	OPT_USE,
	OPT_MINING,
};

static struct option    optlong[] =
{
	{ "help",		no_argument,		0,	OPT_HELP },
	{ "h",		no_argument,		0,	OPT_HELP },
	{ "verbose",	no_argument,		0,	OPT_VERBOSE },
	{ "v",		no_argument,		0,	OPT_VERBOSE },
	{ "i",		required_argument,	0,	OPT_INPUTHEADER },
	{ "nonces",	required_argument,	0,	OPT_NONCES },
	{ "t",		required_argument,	0,	OPT_THREADS },
	{ "n",		required_argument,	0,	OPT_T },
	{ "k",		required_argument,	0,	OPT_B },
	{ "list",		no_argument,		0,	OPT_LIST },
	{ "use",		required_argument,	0,	OPT_USE },
	{ "mining",	no_argument,		0,	OPT_MINING },
	{ 0,		0,			0,	0 },
};

void usage(const char *progname)
{
	printf("Usage: %s [options]\n"
		"A standalone GPU Zcash Equihash solver.\n"
		"\n"
		"Options are:\n"
		"  -h, --help     display this help and exit\n"
		"  -v, --verbose  print verbose messages\n"
		"  -i <input>     140-byte hex block header to solve "
		"(default: all-zero header)\n"
		"  --nonces <nr>  number of nonces to try (default: 1)\n"
		"  -n <n>         equihash n param (only supported value is 200)\n"
		"  -k <k>         equihash k param (only supported value is 9)\n"
		"  --list         list available OpenCL devices by ID (GPUs...)\n"
		"  --use <id>     use GPU <id> (default: 0)\n"
		"  --mining       enable mining mode (solver controlled via "
		"stdin/stdout)\n"
		, progname);
}

int main(int argc, char **argv)
{
	uint8_t             header[ZCASH_BLOCK_HEADER_LEN] = { 0, };
	uint32_t            header_len;
	char		*hex_header = NULL;
	int32_t             i;
	while (-1 != (i = getopt_long_only(argc, argv, "", optlong, 0)))
		switch (i)
		{
		case OPT_HELP:
			usage(argv[0]), exit(0);
			break;
		case OPT_VERBOSE:
			verbose += 1;
			break;
		case OPT_INPUTHEADER:
			hex_header = optarg;
			show_encoded = 1;
			break;
		case OPT_NONCES:
			nr_nonces = parse_num(optarg);
			break;
		case OPT_T:
			gthreads = parse_num(optarg);

			break;
		case OPT_B:
			blocks = parse_num(optarg);
			break;
		case OPT_LIST:
			do_list_devices = 1;
			break;
		case OPT_USE:
			gpu_to_use = parse_num(optarg);
			break;
		case OPT_MINING:
			mining = 1;
			break;
		default:
			fatal("Try '%s --help'\n", argv[0]);
			break;
		}
	if (mining)
		puts("SILENTARMY mining mode ready"), fflush(stdout);
	header_len = parse_header(header, sizeof(header), hex_header);
	init_and_run_opencl(header, header_len);
	return 0;
}



