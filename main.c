#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <CL/cl.h>
#include "blake.h"
#include "_kernel.h"

typedef uint8_t		uchar;
typedef uint32_t	uint;
typedef uint64_t	ulong;
#include "param.h"

#define MIN(A, B)	(((A) < (B)) ? (A) : (B))
#define MAX(A, B)	(((A) > (B)) ? (A) : (B))

int             verbose = 0;
uint32_t	show_encoded = 0;
uint64_t	nr_nonces = 1;
uint32_t	do_list_gpu = 0;
uint32_t	gpu_to_use = 0;

typedef struct  debug_s
{
    uint32_t    dropped_coll;
    uint32_t    dropped_stor;
}               debug_t;

void debug(const char *fmt, ...)
{
    va_list     ap;
    if (!verbose)
        return ;
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
    status = clSetKernelArg(k, a_pos, sizeof (*a), a);
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
    static char		buf[1024];
    uint32_t		i;
    for (i = 0; i < a_len && i + 2 < sizeof (buf); i++)
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
    fatal("Invalid hex char at offset %zd: ...%c...\n", off, c);
    return 0;
}

unsigned nr_compute_units(const char *gpu)
{
    if (!strcmp(gpu, "rx480")) return 36;
    fprintf(stderr, "Unknown GPU: %s\n", gpu);
    return 0;
}

void load_file(const char *fname, char **dat, size_t *dat_len)
{
    struct stat	st;
    int		fd;
    ssize_t	ret;
    if (-1 == (fd = open(fname, O_RDONLY)))
	fatal("%s: %s\n", fname, strerror(errno));
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
    char	        val[2*1024*1024];
    size_t		ret = 0;
    status = clGetProgramBuildInfo(program, device,
	    CL_PROGRAM_BUILD_LOG,
	    sizeof (val),	// size_t param_value_size
	    &val,		// void *param_value
	    &ret);		// size_t *param_value_size_ret
    if (status != CL_SUCCESS)
	fatal("clGetProgramBuildInfo (%d)\n", status);
    fprintf(stderr, "%s\n", val);
}

void dump(const char *fname, void *data, size_t len)
{
    int			fd;
    ssize_t		ret;
    if (-1 == (fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666)))
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
	    sizeof (sizes),	// size_t param_value_size
	    &sizes,		// void *param_value
	    &ret);		// size_t *param_value_size_ret
    if (status != CL_SUCCESS)
	fatal("clGetProgramInfo(sizes) (%d)\n", status);
    if (ret != sizeof (sizes))
	fatal("clGetProgramInfo(sizes) did not fill sizes (%d)\n", status);
    debug("Program binary size is %zd bytes\n", sizes);
    p = (unsigned char *)malloc(sizes);
    status = clGetProgramInfo(program, CL_PROGRAM_BINARIES,
	    sizeof (p),	// size_t param_value_size
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
    status = clGetPlatformInfo(plat, CL_PLATFORM_NAME, sizeof (name), &name,
	    &len);
    if (status != CL_SUCCESS)
	fatal("clGetPlatformInfo (%d)\n", status);
    printf("Devices on platform \"%s\":\n", name);
}

void print_device_info(unsigned i, cl_device_id d)
{
    char	name[1024];
    size_t	len = 0;
    int		status;
    status = clGetDeviceInfo(d, CL_DEVICE_NAME, sizeof (name), &name, &len);
    if (status != CL_SUCCESS)
	fatal("clGetDeviceInfo (%d)\n", status);
    printf("  ID %d: %s\n", i, name);
}

#ifdef ENABLE_DEBUG
uint32_t has_i(uint32_t round, uint8_t *ht, uint32_t row, uint32_t i,
	uint32_t mask, uint32_t *res)
{
    uint32_t	slot;
    uint8_t	*p = (uint8_t *)(ht + row * NR_SLOTS * SLOT_LEN);
    uint32_t	cnt = *(uint32_t *)p;
    cnt = MIN(cnt, NR_SLOTS);
    for (slot = 0; slot < cnt; slot++, p += SLOT_LEN)
      {
	if ((*(uint32_t *)(p + xi_offset_for_round(round) - 4) & mask) ==
		(i & mask))
	  {
	    if (res)
		*res = slot;
	    return 1;
	  }
      }
    return 0;
}

uint32_t has_xi(uint32_t round, uint8_t *ht, uint32_t row, uint32_t xi,
	uint32_t *res)
{
    uint32_t	slot;
    uint8_t	*p = (uint8_t *)(ht + row * NR_SLOTS * SLOT_LEN);
    uint32_t	cnt = *(uint32_t *)p;
    cnt = MIN(cnt, NR_SLOTS);
    for (slot = 0; slot < cnt; slot++, p += SLOT_LEN)
      {
	if ((*(uint32_t *)(p + xi_offset_for_round(round))) == (xi))
	  {
	    if (res)
		*res = slot;
	    return 1;
	  }
      }
    return 0;
}

void examine_ht(unsigned round, cl_command_queue queue, cl_mem buf_ht)
{
    uint8_t     *ht;
    uint8_t	*p;
    if (verbose < 3)
	return ;
    ht = (uint8_t *)malloc(HT_SIZE);
    if (!ht)
	fatal("malloc: %s\n", strerror(errno));
    check_clEnqueueReadBuffer(queue, buf_ht,
	    CL_TRUE,	// cl_bool	blocking_read
	    0,		// size_t	offset
	    HT_SIZE,    // size_t	size
	    ht,	        // void		*ptr
	    0,		// cl_uint	num_events_in_wait_list
	    NULL,	// cl_event	*event_wait_list
	    NULL);	// cl_event	*event
    for (unsigned row = 0; row < NR_ROWS; row++)
      {
	char show = 0;
	uint32_t star = 0;
	if (round == 0)
	  {
	    // i = 0x35c and 0x12d31f collide on first 20 bits
	    show |= has_i(round, ht, row, 0x35c, 0xffffffffUL, &star);
	    show |= has_i(round, ht, row, 0x12d31f, 0xffffffffUL, &star);
	  }
	if (round == 1)
	  {
	    show |= has_xi(round, ht, row, 0xf0937683, &star);
	    show |= (row < 256);
	  }
	if (round == 2)
	  {
	    show |= has_xi(round, ht, row, 0x3519d2e0, &star);
	    show |= (row < 256);
	  }
	if (round == 3)
	  {
	    show |= has_xi(round, ht, row, 0xd6950b66, &star);
	    show |= (row < 256);
	  }
	if (round == 4)
	  {
	    show |= has_xi(round, ht, row, 0xa92db6ab, &star);
	    show |= (row < 256);
	  }
	if (round == 5)
	  {
	    show |= has_xi(round, ht, row, 0x2daaa343, &star);
	    show |= (row < 256);
	  }
	if (round == 6)
	  {
	    show |= has_xi(round, ht, row, 0x53b9dd5d, &star);
	    show |= (row < 256);
	  }
	if (round == 7)
	  {
	    show |= has_xi(round, ht, row, 0xb9d374fe, &star);
	    show |= (row < 256);
	  }
	if (round == 8)
	  {
	    show |= has_xi(round, ht, row, 0x005ae381, &star);
	    show |= (row < 256);
	  }
	if (show)
	  {
	    debug("row %#x:\n", row);
	    uint32_t cnt = *(uint32_t *)(ht + row * NR_SLOTS * SLOT_LEN);
	    cnt = MIN(cnt, NR_SLOTS);
	    for (unsigned slot = 0; slot < cnt; slot++)
		if (slot < NR_SLOTS)
		  {
		    p = ht + row * NR_SLOTS * SLOT_LEN + slot * SLOT_LEN;
		    debug("%c%02x ", (star == slot) ? '*' : ' ', slot);
		    for (unsigned i = 0; i < 4; i++, p++)
			!slot ? debug("%02x", *p) : debug("__");
		    uint64_t val[3] = {0,};
		    for (unsigned i = 0; i < 28; i++, p++)
		      {
			if (i == round / 2 * 4 + 4)
			  {
			    val[0] = *(uint64_t *)(p + 0);
			    val[1] = *(uint64_t *)(p + 8);
			    val[2] = *(uint64_t *)(p + 16);
			    debug(" | ");
			  }
			else if (!(i % 4))
			    debug(" ");
			debug("%02x", *p);
		      }
		    val[0] = (val[0] >> 4) | (val[1] << (64 - 4));
		    val[1] = (val[1] >> 4) | (val[2] << (64 - 4));
		    val[2] = (val[2] >> 4);
		    debug("\n");
		  }
	  }
      }
    free(ht);
}
#else
void examine_ht(unsigned round, cl_command_queue queue, cl_mem buf_ht)
{
    (void)round;
    (void)queue;
    (void)buf_ht;
}
#endif

void examine_dbg(cl_command_queue queue, cl_mem buf_dbg, size_t dbg_size)
{
    debug_t     *dbg;
    size_t      dropped_coll_total, dropped_stor_total;
    if (verbose < 2)
	return ;
    dbg = (debug_t *)malloc(dbg_size);
    if (!dbg)
	fatal("malloc: %s\n", strerror(errno));
    check_clEnqueueReadBuffer(queue, buf_dbg,
            CL_TRUE,	// cl_bool	blocking_read
            0,		// size_t	offset
            dbg_size,   // size_t	size
            dbg,	// void		*ptr
            0,		// cl_uint	num_events_in_wait_list
            NULL,	// cl_event	*event_wait_list
            NULL);	// cl_event	*event
    dropped_coll_total = dropped_stor_total = 0;
    for (unsigned tid = 0; tid < dbg_size / sizeof (*dbg); tid++)
      {
        dropped_coll_total += dbg[tid].dropped_coll;
        dropped_stor_total += dbg[tid].dropped_stor;
	if (0 && (dbg[tid].dropped_coll || dbg[tid].dropped_stor))
	    debug("thread %6d: dropped_coll %zd dropped_stor %zd\n", tid,
		    dbg[tid].dropped_coll, dbg[tid].dropped_stor);
      }
    debug("Dropped: %zd (coll) %zd (stor)\n",
            dropped_coll_total, dropped_stor_total);
    free(dbg);
}

size_t select_work_size_blake(void)
{
    size_t              work_size =
        64 * /* thread per wavefront */
        BLAKE_WPS * /* wavefront per simd */
        4 * /* simd per compute unit */
        nr_compute_units("rx480");
    // Make the work group size a multiple of the nr of wavefronts, while
    // dividing the number of inputs. This results in the worksize being a
    // power of 2.
    while (NR_INPUTS % work_size)
        work_size += 64;
    //debug("Blake: work size %zd\n", work_size);
    return work_size;
}

void init_ht(cl_command_queue queue, cl_kernel k_init_ht, cl_mem buf_ht)
{
    size_t      global_ws = NR_ROWS;
    size_t      local_ws = 64;
    cl_int      status;
#if 0
    uint32_t    pat = -1;
    status = clEnqueueFillBuffer(queue, buf_ht, &pat, sizeof (pat), 0,
            NR_ROWS * NR_SLOTS * SLOT_LEN,
            0,		// cl_uint	num_events_in_wait_list
            NULL,	// cl_event	*event_wait_list
            NULL);	// cl_event	*event
    if (status != CL_SUCCESS)
        fatal("clEnqueueFillBuffer (%d)\n", status);
#endif
    status = clSetKernelArg(k_init_ht, 0, sizeof (buf_ht), &buf_ht);
    if (status != CL_SUCCESS)
        fatal("clSetKernelArg (%d)\n", status);
    check_clEnqueueNDRangeKernel(queue, k_init_ht,
            1,		// cl_uint	work_dim
            NULL,	// size_t	*global_work_offset
            &global_ws,	// size_t	*global_work_size
            &local_ws,	// size_t	*local_work_size
            0,		// cl_uint	num_events_in_wait_list
            NULL,	// cl_event	*event_wait_list
            NULL);	// cl_event	*event
}

/*
** Print on stdout a hex representation of the encoded solution as per the
** zcash protocol specs (512 x 21-bit inputs).
**
** inputs       array of 32-bit inputs
** n            number of elements in array
*/
void print_encoded_sol(uint32_t *inputs, uint32_t n)
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
            printf("%02x", x);
            x = x_bits_used = 0;
          }
      }
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
    if (*nonce < (1UL << 32))
	fprintf(stderr, " 0x%lx:", *nonce);
    for (unsigned i = 0; i < show_n_sols; i++)
	fprintf(stderr, " %x", values[i]);
    fprintf(stderr, "%s\n", (show_n_sols != (1 << PARAM_K) ? "..." : ""));
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
*/
void print_sols(sols_t *all_sols, uint64_t *nonce, uint32_t nr_valid_sols)
{
    uint8_t		*valid_sols;
    uint32_t		counted;
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
    // sort the solutions amongst each other, to make silentarmy's output
    // deterministic and testable
    qsort(valid_sols, nr_valid_sols, SOL_SIZE, sol_cmp);
    for (uint32_t i = 0; i < nr_valid_sols; i++)
      {
	uint32_t	*inputs = (uint32_t *)(valid_sols + i * SOL_SIZE);
	if (show_encoded)
	    print_encoded_sol(inputs, 1 << PARAM_K);
	if (verbose)
	    print_sol(inputs, nonce);
      }
    free(valid_sols);
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
	    return ;
}

/*
** If solution is invalid return 0. If solution is valid, sort the inputs
** and return 1.
*/
uint32_t verify_sol(sols_t *sols, unsigned sol_i)
{
    uint32_t	*inputs = sols->values[sol_i];
    uint32_t	seen_len = (1 << (PREFIX + 1)) / 8;
    uint8_t	seen[seen_len];
    uint32_t	i;
    uint8_t	tmp;
    // look for duplicate inputs
    memset(seen, 0, seen_len);
    for (i = 0; i < (1 << PARAM_K); i++)
      {
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

/*
** Return the number of valid solutions.
*/
uint32_t verify_sols(cl_command_queue queue, cl_mem buf_sols, uint64_t *nonce)
{
    sols_t	*sols;
    uint32_t	nr_valid_sols;
    sols = (sols_t *)malloc(sizeof (*sols));
    if (!sols)
	fatal("malloc: %s\n", strerror(errno));
    check_clEnqueueReadBuffer(queue, buf_sols,
	    CL_TRUE,	// cl_bool	blocking_read
	    0,		// size_t	offset
	    sizeof (*sols),	// size_t	size
	    sols,	// void		*ptr
	    0,		// cl_uint	num_events_in_wait_list
	    NULL,	// cl_event	*event_wait_list
	    NULL);	// cl_event	*event
    if (sols->nr > MAX_SOLS)
      {
	fprintf(stderr, "%d (probably invalid) solutions were dropped!\n",
		sols->nr - MAX_SOLS);
	sols->nr = MAX_SOLS;
      }
    nr_valid_sols = 0;
    for (unsigned sol_i = 0; sol_i < sols->nr; sol_i++)
	nr_valid_sols += verify_sol(sols, sol_i);
    print_sols(sols, nonce, nr_valid_sols);
    fprintf(stderr, "Nonce %s: %d sol%s\n",
	    s_hexdump(nonce, ZCASH_NONCE_LEN), nr_valid_sols,
	    nr_valid_sols == 1 ? "" : "s");
    debug("Stats: %d likely invalids\n", sols->likely_invalids);
    free(sols);
    return nr_valid_sols;
}

/*
** Attempt to find Equihash solutions for the given Zcash block header and
** nonce. The 'header' passed in argument is either:
**
** - a 140-byte full header specifying the nonce, or
** - a 108-byte nonceless header, implying a nonce of 32 zero bytes
**
** In both cases the function constructs the full block header to solve by
** adding the value of 'nonce' to the nonce in 'header'. This allows
** repeatedly calling this fuction while changing only the value of 'nonce'
** to attempt different Equihash problems.
**
** header	must be a buffer allocated with ZCASH_BLOCK_HEADER_LEN bytes
** header_len	number of bytes initialized in header (either 140 or 108)
**
** Return the number of solutions found.
*/
uint32_t solve_equihash(cl_context ctx, cl_command_queue queue,
	cl_kernel k_init_ht, cl_kernel *k_rounds, cl_kernel k_sols,
	cl_mem *buf_ht, cl_mem buf_sols, cl_mem buf_dbg, size_t dbg_size,
	uint8_t *header, size_t header_len, uint64_t nonce)
{
    blake2b_state_t     blake;
    cl_mem              buf_blake_st;
    size_t		global_ws;
    size_t              local_work_size = 64;
    uint32_t		sol_found = 0;
    uint64_t		*nonce_ptr;
    assert(header_len == ZCASH_BLOCK_HEADER_LEN ||
	    header_len == ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN);
    nonce_ptr = (uint64_t *)(header + ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN);
    if (header_len == ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN)
	memset(nonce_ptr, 0, ZCASH_NONCE_LEN);
    // add the nonce
    *nonce_ptr += nonce;
    debug("\nSolving nonce %s\n", s_hexdump(nonce_ptr, ZCASH_NONCE_LEN));
    // Process first BLAKE2b-400 block
    zcash_blake2b_init(&blake, ZCASH_HASH_LEN, PARAM_N, PARAM_K);
    zcash_blake2b_update(&blake, header, 128, 0);
    buf_blake_st = check_clCreateBuffer(ctx, CL_MEM_READ_ONLY |
	    CL_MEM_COPY_HOST_PTR, sizeof (blake.h), &blake.h);
    for (unsigned round = 0; round < PARAM_K; round++)
      {
	if (verbose > 1)
	    debug("Round %d\n", round);
	if (round < 2)
	    init_ht(queue, k_init_ht, buf_ht[round % 2]);
	if (!round)
	  {
	    check_clSetKernelArg(k_rounds[round], 0, &buf_blake_st);
	    check_clSetKernelArg(k_rounds[round], 1, &buf_ht[round % 2]);
	    global_ws = select_work_size_blake();
	  }
	else
	  {
	    check_clSetKernelArg(k_rounds[round], 0, &buf_ht[(round - 1) % 2]);
	    check_clSetKernelArg(k_rounds[round], 1, &buf_ht[round % 2]);
	    global_ws = NR_ROWS;
	  }
	check_clSetKernelArg(k_rounds[round], 2, &buf_dbg);
	if (round == PARAM_K - 1)
	    check_clSetKernelArg(k_rounds[round], 3, &buf_sols);
	check_clEnqueueNDRangeKernel(queue, k_rounds[round], 1, NULL,
		&global_ws, &local_work_size, 0, NULL, NULL);
	examine_ht(round, queue, buf_ht[round % 2]);
	examine_dbg(queue, buf_dbg, dbg_size);
      }
    check_clSetKernelArg(k_sols, 0, &buf_ht[0]);
    check_clSetKernelArg(k_sols, 1, &buf_ht[1]);
    check_clSetKernelArg(k_sols, 2, &buf_sols);
    global_ws = NR_ROWS;
    check_clEnqueueNDRangeKernel(queue, k_sols, 1, NULL,
	    &global_ws, &local_work_size, 0, NULL, NULL);
    sol_found = verify_sols(queue, buf_sols, nonce_ptr);
    clReleaseMemObject(buf_blake_st);
    return sol_found;
}

void run_opencl(uint8_t *header, size_t header_len, cl_context ctx,
        cl_command_queue queue, cl_kernel k_init_ht, cl_kernel *k_rounds,
	cl_kernel k_sols)
{
    cl_mem              buf_ht[2], buf_sols, buf_dbg;
    void                *dbg = NULL;
#ifdef ENABLE_DEBUG
    size_t              dbg_size = NR_ROWS * sizeof (debug_t);
#else
    size_t              dbg_size = 1 * sizeof (debug_t);
#endif
    uint64_t		nonce;
    uint64_t		total;
    fprintf(stderr, "Hash tables will use %.1f MB\n", 2.0 * HT_SIZE / 1e6);
    // Set up buffers for the host and memory objects for the kernel
    if (!(dbg = calloc(dbg_size, 1)))
	fatal("malloc: %s\n", strerror(errno));
    buf_dbg = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE |
	    CL_MEM_COPY_HOST_PTR, dbg_size, dbg);
    buf_ht[0] = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, HT_SIZE, NULL);
    buf_ht[1] = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, HT_SIZE, NULL);
    buf_sols = check_clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof (sols_t),
	    NULL);
    fprintf(stderr, "Running...\n");
    // Solve Equihash for a few nonces
    total = 0;
    uint64_t t0 = now();
    for (nonce = 0; nonce < nr_nonces; nonce++)
	total += solve_equihash(ctx, queue, k_init_ht, k_rounds, k_sols, buf_ht,
		buf_sols, buf_dbg, dbg_size, header, header_len, nonce);
    uint64_t t1 = now();
    fprintf(stderr, "Total %ld solutions in %.1f ms (%.1f Sol/s)\n",
	    total, (t1 - t0) / 1e3, total / ((t1 - t0) / 1e6));
    // Clean up
    if (dbg)
        free(dbg);
    clReleaseMemObject(buf_dbg);
    clReleaseMemObject(buf_ht[0]);
    clReleaseMemObject(buf_ht[1]);
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
    cl_device_type	typ = CL_DEVICE_TYPE_GPU; // only look for GPUs
    cl_uint		nr_devs = 0;
    cl_device_id	*devices;
    cl_int		status;
    unsigned		found = 0;
    unsigned		i;
    if (do_list_gpu)
	print_platform_info(plat);
    status = clGetDeviceIDs(plat, typ, 0, NULL, &nr_devs);
    if (status != CL_SUCCESS)
	fatal("clGetDeviceIDs (%d)\n", status);
    if (nr_devs == 0)
	return 0;
    devices = (cl_device_id *)malloc(nr_devs * sizeof (*devices));
    status = clGetDeviceIDs(plat, typ, nr_devs, devices, NULL);
    if (status != CL_SUCCESS)
	fatal("clGetDeviceIDs (%d)\n", status);
    i = 0;
    while (i < nr_devs)
      {
	if (do_list_gpu)
	    print_device_info(*nr_devs_total, devices[i]);
	else if (*nr_devs_total == gpu_to_use)
	  {
	    found = 1;
	    *plat_id = plat;
	    *dev_id = devices[i];
	    break ;
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
    platforms = (cl_platform_id *)malloc(nr_platforms * sizeof (*platforms));
    if (!platforms)
	fatal("malloc: %s\n", strerror(errno));
    status = clGetPlatformIDs(nr_platforms, platforms, NULL);
    if (status != CL_SUCCESS)
	fatal("clGetPlatformIDs (%d)\n", status);
    i = nr_devs_total = 0;
    while (i < nr_platforms)
      {
	if (scan_platform(platforms[i], &nr_devs_total, plat_id, dev_id))
	    break ;
	i++;
      }
    if (do_list_gpu)
	exit(0);
    debug("Using GPU device ID %d\n", gpu_to_use);
    free(platforms);
}

void init_and_run_opencl(uint8_t *header, size_t header_len)
{
    cl_platform_id	plat_id = 0;
    cl_device_id	dev_id = 0;
    cl_kernel		k_rounds[PARAM_K];
    cl_int		status;
    scan_platforms(&plat_id, &dev_id);
    if (!plat_id || !dev_id)
	fatal("Selected GPU (ID %d) not found; see --list-gpu\n", gpu_to_use);
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
    cl_program program;
    const char *source;
    size_t source_len;
    //load_file("kernel.cl", &source, &source_len);
    source = ocl_code;
    source_len = strlen(ocl_code);
    program = clCreateProgramWithSource(context, 1, (const char **)&source,
	    &source_len, &status);
    if (status != CL_SUCCESS || !program)
	fatal("clCreateProgramWithSource (%d)\n", status);
    /* Build program. */
    fprintf(stderr, "Building program\n");
    status = clBuildProgram(program, 1, &dev_id,
	    "", // compile options
	    NULL, NULL);
    if (status != CL_SUCCESS)
      {
        warn("OpenCL build failed (%d). Build log follows:\n", status);
        get_program_build_log(program, dev_id);
	exit(1);
      }
    //get_program_bins(program);
    // Create kernel objects
    cl_kernel k_init_ht = clCreateKernel(program, "kernel_init_ht", &status);
    if (status != CL_SUCCESS || !k_init_ht)
	fatal("clCreateKernel (%d)\n", status);
    for (unsigned round = 0; round < PARAM_K; round++)
      {
	char	name[128];
	snprintf(name, sizeof (name), "kernel_round%d", round);
	k_rounds[round] = clCreateKernel(program, name, &status);
	if (status != CL_SUCCESS || !k_rounds[round])
	    fatal("clCreateKernel (%d)\n", status);
      }
    cl_kernel k_sols = clCreateKernel(program, "kernel_sols", &status);
    if (status != CL_SUCCESS || !k_sols)
	fatal("clCreateKernel (%d)\n", status);
    // Run
    run_opencl(header, header_len, context, queue, k_init_ht, k_rounds, k_sols);
    // Release resources
    assert(CL_SUCCESS == 0);
    status = CL_SUCCESS;
    status |= clReleaseKernel(k_init_ht);
    for (unsigned round = 0; round < PARAM_K; round++)
	status |= clReleaseKernel(k_rounds[round]);
    status |= clReleaseProgram(program);
    status |= clReleaseCommandQueue(queue);
    status |= clReleaseContext(context);
    if (status)
	fprintf(stderr, "Cleaning resources failed\n");
}

void print_header(uint8_t *h, size_t len)
{
    for (uint32_t i = 0; i < len; i++)
	printf("%02x", h[i]);
    printf(" (%zd bytes)\n", len);
}

uint32_t parse_header(uint8_t *h, size_t h_len, const char *hex)
{
    size_t      hex_len;
    size_t      bin_len;
    size_t	opt0 = ZCASH_BLOCK_HEADER_LEN;
    size_t	opt1 = ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN;
    size_t      i;
    if (!hex)
      {
	if (!do_list_gpu)
	    fprintf(stderr, "Solving default all-zero %zd-byte header\n", opt0);
	return opt1;
      }
    hex_len = strlen(hex);
    bin_len = hex_len / 2;
    if (hex_len % 2)
	fatal("Error: input header must be an even number of hex digits\n");
    if (bin_len != opt0 && bin_len != opt1)
	fatal("Error: input header must be either a %zd-byte full header, "
		"or a %zd-byte nonceless header\n", opt0, opt1);
    assert(bin_len <= h_len);
    for (i = 0; i < bin_len; i ++)
	h[i] = hex2val(hex, i * 2) * 16 + hex2val(hex, i * 2 + 1);
    if (bin_len == opt0)
	while (--i >= bin_len - 12)
	    if (h[i])
		fatal("Error: last 12 bytes of full header (ie. last 12 "
			"bytes of 32-byte nonce) must be zero due to an "
			"optimization in my BLAKE2b implementation\n");
    return bin_len;
}

enum
{
    OPT_HELP,
    OPT_VERBOSE,
    OPT_INPUTHEADER,
    OPT_NONCES,
    OPT_THREADS,
    OPT_N,
    OPT_K,
    OPT_LIST_GPU,
    OPT_USE,
};

static struct option    optlong[] =
{
      {"help",		no_argument,		0,	OPT_HELP},
      {"h",		no_argument,		0,	OPT_HELP},
      {"verbose",	no_argument,		0,	OPT_VERBOSE},
      {"v",		no_argument,		0,	OPT_VERBOSE},
      {"i",		required_argument,	0,	OPT_INPUTHEADER},
      {"nonces",	required_argument,	0,	OPT_NONCES},
      {"t",		required_argument,	0,	OPT_THREADS},
      {"n",		required_argument,	0,	OPT_N},
      {"k",		required_argument,	0,	OPT_K},
      {"list-gpu",	no_argument,		0,	OPT_LIST_GPU},
      {"use",		required_argument,	0,	OPT_USE},
      {0,		0,			0,	0},
};

void usage(const char *progname)
{
    printf("Usage: %s [options]\n"
	    "Silentarmy is a GPU Zcash Equihash solver.\n"
	    "\n"
	    "Options are:\n"
            "  -h, --help     display this help and exit\n"
            "  -v, --verbose  print verbose messages\n"
            "  -i <input>     hex block header to solve; either a 140-byte "
	    "full header,\n"
	    "                 or a 108-byte nonceless header with implicit "
	    "zero nonce\n"
	    "                 (default: all-zero header)\n"
            "  --nonces <nr>  number of nonces to try (default: 1)\n"
            "  -n <n>         equihash n param (only supported value is 200)\n"
            "  -k <k>         equihash k param (only supported value is 9)\n"
            "  --list-gpu     list available GPU devices\n"
            "  --use <id>     use GPU <id> (default: 0)\n"
            , progname);
}

void tests(void)
{
    // if NR_ROWS_LOG is smaller, there is not enough space to store all bits
    // of Xi in a 32-byte slot
    assert(NR_ROWS_LOG >= 12);
}

int main(int argc, char **argv)
{
    uint8_t             header[ZCASH_BLOCK_HEADER_LEN] = {0,};
    uint32_t            header_len;
    char		*hex_header = NULL;
    int32_t             i;
    while (-1 != (i = getopt_long_only(argc, argv, "", optlong, 0)))
        switch (i)
          {
            case OPT_HELP:
                usage(argv[0]), exit(0);
                break ;
            case OPT_VERBOSE:
                verbose += 1;
                break ;
            case OPT_INPUTHEADER:
		hex_header = optarg;
		show_encoded = 1;
                break ;
	    case OPT_NONCES:
		nr_nonces = parse_num(optarg);
		break ;
            case OPT_THREADS:
                // ignored, this is just to conform to the contest CLI API
                break ;
            case OPT_N:
                if (PARAM_N != parse_num(optarg))
                    fatal("Unsupported n (must be %d)\n", PARAM_N);
                break ;
            case OPT_K:
                if (PARAM_K != parse_num(optarg))
                    fatal("Unsupported k (must be %d)\n", PARAM_K);
                break ;
	    case OPT_LIST_GPU:
		do_list_gpu = 1;
		break ;
	    case OPT_USE:
		gpu_to_use = parse_num(optarg);
		break ;
            default:
                fatal("Try '%s --help'\n", argv[0]);
                break ;
          }
    tests();
    header_len = parse_header(header, sizeof (header), hex_header);
    init_and_run_opencl(header, header_len);
    return 0;
}
