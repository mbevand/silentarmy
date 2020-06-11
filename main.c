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
#define VERUS_WORKSIZE 0x90000
#define VERUS_PACKET 0x10000
#define MAIN_THREADS 64

static u128 *data_key = NULL;


int         verbose = 0;
uint32_t	show_encoded = 0;
uint64_t	nr_nonces = 1;
uint32_t	do_list_devices = 0;
uint32_t	gpu_to_use = 0;
uint32_t	mining = 0;
uint32_t    blocks = VERUS_WORKSIZE;
uint32_t    gthreads = MAIN_THREADS;
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


/*
** Verify if the solution's block hash is under the target, and if yes print
** it formatted as:
** "sol: <job_id> <ntime> <nonce_rightpart> <solSize+sol>"
**
** Return 1 iff the block hash is under the target.
*/


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


#ifndef DEFAULT_NUM_MINING_MODE_THREADS
#define DEFAULT_NUM_MINING_MODE_THREADS 1
#define MAX_NUM_MINING_MODE_THREADS 16
#endif
uint32_t num_mining_mode_threads = DEFAULT_NUM_MINING_MODE_THREADS;
#ifdef WIN32
CRITICAL_SECTION cs;
#endif
struct mining_mode_thread_args {
	cl_device_id dev_id;
	cl_context ctx;
	cl_command_queue queue;

	//
	uint8_t     header[ZCASH_BLOCK_HEADER_LEN];
	uint8_t		target[32];
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
	uint8_t		target[32];
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
	cl_mem          key_const_d, data_keylarge_d, blockhash_half_d, target_d, resnonces_d, startNonce_d;
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
	data_keylarge_d = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint8_t) * VERUS_KEY_SIZE * VERUS_PACKET, NULL);
	nonce_ptr = (uint64_t *)(header + ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN);
#ifdef WIN32
	InitializeCriticalSection(&cs);
#endif
	puts("SILENTARMY mining mode ready");
	fflush(stdout);
#ifdef WIN32
	SetConsoleOutputCP(65001);
#endif
	int started = 0;
	for (i = 0; ; i++)
	{
		int changed = 0;
		// iteration #0 always reads a job or else there is nothing to do

		nonce_sum[0] = 0;
		if (read_last_line(line, sizeof(line), !i)) {
			changed = 1; started = 1;
#ifdef WIN32
			EnterCriticalSection(&cs);
#endif
			mining_parse_job(line,
				target, sizeof(target),
				job_id, sizeof(job_id),
				header, ZCASH_BLOCK_HEADER_LEN,
				&fixed_nonce_bytes);
#ifdef WIN32
			LeaveCriticalSection(&cs);
#endif
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


			}

			pnonces[0] = 0xfffffffful;
			status = clEnqueueWriteBuffer(queue, resnonces_d, CL_TRUE, 0, sizeof(uint32_t) * 1, pnonces, 0, NULL, NULL);
			if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);
			status = clEnqueueWriteBuffer(queue, startNonce_d, CL_TRUE, 0, sizeof(uint32_t) * 1, nonce_sum, 0, NULL, NULL);
			if (status != CL_SUCCESS)printf("clEnqueueWriteBuffer (%d)\n", status);


			check_clSetKernelArg(k_verus[1], 0, &startNonce_d);
			check_clSetKernelArg(k_verus[1], 1, &blockhash_half_d);
			check_clSetKernelArg(k_verus[1], 2, &data_keylarge_d);
			check_clSetKernelArg(k_verus[1], 3, &key_const_d);
			check_clSetKernelArg(k_verus[1], 4, &target_d);
			check_clSetKernelArg(k_verus[1], 5, &resnonces_d);

			global_ws = blocks;
			local_work_size = gthreads;

			check_clEnqueueNDRangeKernel(queue, k_verus[1], 1, NULL,
				&global_ws, &local_work_size, 0, NULL, NULL);

			num_sols = verify_nonce(queue, resnonces_d, header,
				fixed_nonce_bytes, target, job_id, shares, blockhash_half, pnonces);

			total += blocks;
			total_shares += num_sols;
			if ((nonce_sum[0] + blocks) < 0xfffffffful)
				nonce_sum[0] += blocks;
			else
				nonce_sum[0] = 0;


			if ((t1 = now()) > t0 + status_period)
			{
#ifdef WIN32
				EnterCriticalSection(&cs);
				t0 = t1;
#endif
				printf("status: %" PRId64 " %" PRId64 "\n", total / 1000000, total_shares);
				fflush(stdout);
#ifdef WIN32
				LeaveCriticalSection(&cs);
#endif
				//fprintf(stderr, " (%d kh/s)\n",
				//	(t3 - t2) / 1e3, total / ((t3 - t2) / 1e6) / 1000);
				//fflush(stdout);
			}
		}
	}



}


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

	char* yy = (char*)malloc(100);  sprintf(yy, "-D TOTAL_MAX=%d -D THREADS=%d", VERUS_PACKET - 1, MAIN_THREADS);

	status = clBuildProgram(program, 1, &dev_id, yy, // compile options
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


	k_verus[1] = clCreateKernel(program, "verus_gpu_hash", &status);
	if (status != CL_SUCCESS || !k_verus[1])
		fatal("clCreateKernel2 (%d)\n", status);


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


