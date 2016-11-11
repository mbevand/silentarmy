#ifndef _SILENTARMY_H_
#define _SILENTARMY_H_

#include <stdint.h>

#include <CL/cl.h>

#include "silentarmy-types.h"
#include "param.h"
extern const char *ocl_code;

typedef struct  debug_s {
  uint32_t dropped_coll;
  uint32_t dropped_stor;
} debug_t;

void debug(const char *fmt, ...);
void warn(const char *fmt, ...);
void fatal(const char *fmt, ...);
extern int verbose;

char *s_hexdump(const void *_a, uint32_t a_len);

uint64_t now(void);

void get_program_build_log(cl_program program, cl_device_id device);

cl_mem check_clCreateBuffer(cl_context ctx, cl_mem_flags flags, size_t size,
			    void *host_ptr);

void check_clSetKernelArg(cl_kernel k, cl_uint a_pos, cl_mem *a);

void check_clEnqueueNDRangeKernel(cl_command_queue queue, cl_kernel k, cl_uint
				  work_dim, const size_t *global_work_offset, const size_t
				  *global_work_size, const size_t *local_work_size, cl_uint
				  num_events_in_wait_list, const cl_event *event_wait_list, cl_event
				  *event);

void check_clEnqueueReadBuffer(cl_command_queue queue, cl_mem buffer, cl_bool
			       blocking_read, size_t offset, size_t size, void *ptr, cl_uint
			       num_events_in_wait_list, const cl_event *event_wait_list, cl_event
			       *event);

uint32_t solve_equihash(cl_context ctx, cl_command_queue queue,
			cl_kernel k_init_ht, cl_kernel *k_rounds, cl_kernel k_sols,
			cl_mem *buf_ht, cl_mem buf_sols, cl_mem buf_dbg, size_t dbg_size,
			uint8_t *header, size_t header_len, char do_increment,
			size_t fixed_nonce_bytes, uint8_t *target, char *job_id,
			uint32_t *shares, bool verify);

uint32_t verify_sol(sols_t *sols, unsigned sol_i);

void check_header_zero_pad(uint8_t *header);

#endif /* SILENTARMY_H */
