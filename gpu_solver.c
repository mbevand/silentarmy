/**
 * @file gpu_solver.c
 * @brief GPU solver heavily based on main.c module by Marc Bevand
 *
 * MIT License
 *
 * Copyright (c) 2016 Jan Capek
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <CL/cl.h>

#include "blake.h"
#include "silentarmy.h"
#include "param.h"

/**
 * GPU solver instance
 */
struct gpu_solver {
  cl_device_id *devices;
  cl_context context;
  cl_command_queue queue;
  cl_program program;
  cl_kernel k_init_ht;
  cl_kernel k_rounds[PARAM_K];
  cl_kernel k_sols;

  cl_mem buf_ht[2];
  cl_mem buf_sols;
  cl_mem buf_dbg;
  void  *dbg;
  size_t dbg_size;
};


struct gpu_solver__encoded_solution {
  uint8_t bytes[(1 << PARAM_K) * (PREFIX + 1) / 8];
};


/**
 * @param *sol - solution that is to be encoded
 * @param count - number entries in sol array
 * @param *encoded_sol - output parameter for storing the solution
 */
void gpu_solver__encode_sol(uint32_t *sol, size_t count, uint8_t *encoded_sol)
{
  size_t byte_pos = 0;
  // TODO make this unsigned
  int32_t bits_left = PREFIX + 1;
  uint8_t x = 0;
  uint8_t x_bits_used = 0;
  while (byte_pos < count) {
    if (bits_left >= 8 - x_bits_used) {
      x |= sol[byte_pos] >> (bits_left - 8 + x_bits_used);
      bits_left -= 8 - x_bits_used;
      x_bits_used = 8;
    }
    else if (bits_left > 0) {
      uint32_t mask = ~(-1 << (8 - x_bits_used));
      mask = ((~mask) >> bits_left) & mask;
      x |= (sol[byte_pos] << (8 - x_bits_used - bits_left)) & mask;
      x_bits_used += bits_left;
      bits_left = 0;
    }
    else if (bits_left <= 0) {
      assert(!bits_left);
      byte_pos++;
      bits_left = PREFIX + 1;
    }
    if (x_bits_used == 8) {
      *encoded_sol = x;
      encoded_sol++;
      x = x_bits_used = 0;
    }
  }
}


int gpu_solver__init(struct gpu_solver *self, uint32_t gpu_to_use)
{
  int result = 0;
  cl_uint num_platforms;
  cl_uint nr_devs = 0;

  cl_int status = clGetPlatformIDs(0, NULL, &num_platforms);
  if (status != CL_SUCCESS)
    fatal("Cannot get OpenCL platforms! (%d)\n", status);
  debug("Found %d OpenCL platform(s)\n", num_platforms);
  if (num_platforms == 0) {
    result = -1;
    goto init_failed;
  }
  cl_platform_id* platforms = (cl_platform_id *)
    malloc(num_platforms * sizeof (cl_platform_id));
  status = clGetPlatformIDs(num_platforms, platforms, NULL);
  if (status != CL_SUCCESS)
    fatal("clGetPlatformIDs (%d)\n", status);
  // always select first platform
  cl_platform_id platform = platforms[0];
  free(platforms);
  // Query the platform and choose the first GPU device if has one
  status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, NULL, &nr_devs);
  if (status != CL_SUCCESS)
    fatal("clGetDeviceIDs (%d)\n", status);
  if (nr_devs == 0)
    fatal("No GPU device available\n");
  self->devices = (cl_device_id*)malloc(nr_devs * sizeof(*self->devices));
  status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, nr_devs, self->devices,
			  NULL);
  if (status != CL_SUCCESS)
    fatal("clGetDeviceIDs (%d)\n", status);

  /* Create context.*/
  if (gpu_to_use >= nr_devs) {
    fatal("%d is an invalid GPU ID; see --list-gpu\n", gpu_to_use);
  }
  self->context = clCreateContext(NULL, 1, self->devices + gpu_to_use,
				  NULL, NULL, &status);
  if (status != CL_SUCCESS || !self->context)
    fatal("clCreateContext (%d)\n", status);
  /* Creating command queue associate with the context.*/
  self->queue = clCreateCommandQueue(self->context, self->devices[gpu_to_use],
				     0, &status);
  if (status != CL_SUCCESS || !self->queue)
    fatal("clCreateCommandQueue (%d)\n", status);
  /* Create program object */
  const char *source;
  size_t source_len;

  source = ocl_code;
  source_len = strlen(ocl_code);
  self->program = clCreateProgramWithSource(self->context, 1, (const char **)&source,
					    &source_len, &status);
  if (status != CL_SUCCESS || !self->program)
    fatal("clCreateProgramWithSource (%d)\n", status);
  /* Build program. */
  fprintf(stderr, "Building program\n");
  status = clBuildProgram(self->program, 1, self->devices + gpu_to_use,
			  "", // compile options
			  NULL, NULL);
  if (status != CL_SUCCESS) {
    warn("OpenCL build failed (%d). Build log follows:\n", status);
    get_program_build_log(self->program, self->devices[gpu_to_use]);
    goto init_failed;
  }
  // Create kernel objects
  self->k_init_ht = clCreateKernel(self->program, "kernel_init_ht", &status);
  if (status != CL_SUCCESS || !self->k_init_ht)
    fatal("clCreateKernel (%d)\n", status);
  for (unsigned round = 0; round < PARAM_K; round++) {
    char name[128];
    snprintf(name, sizeof (name), "kernel_round%d", round);
    self->k_rounds[round] = clCreateKernel(self->program, name, &status);
    if (status != CL_SUCCESS || !self->k_rounds[round])
      fatal("clCreateKernel (%d)\n", status);
  }
  self->k_sols = clCreateKernel(self->program, "kernel_sols", &status);
  if (status != CL_SUCCESS || !self->k_sols) {
    fatal("clCreateKernel (%d)\n", status);
  }

#ifdef ENABLE_DEBUG
  self->dbg_size = NR_ROWS * sizeof(debug_t);
#else
  self->dbg_size = 1 * sizeof(debug_t);
#endif

  fprintf(stderr, "Hash tables will use %.1f MB\n", 2.0 * HT_SIZE / 1e6);
  // Set up buffers for the host and memory objects for the kernel
  if (!(self->dbg = calloc(self->dbg_size, 1)))
    fatal("malloc: %s\n", strerror(errno));
  self->buf_dbg = check_clCreateBuffer(self->context, CL_MEM_READ_WRITE |
				       CL_MEM_COPY_HOST_PTR, self->dbg_size,
				       self->dbg);
  self->buf_ht[0] = check_clCreateBuffer(self->context, CL_MEM_READ_WRITE,
					 HT_SIZE, NULL);
  self->buf_ht[1] = check_clCreateBuffer(self->context, CL_MEM_READ_WRITE,
					 HT_SIZE, NULL);
  self->buf_sols = check_clCreateBuffer(self->context, CL_MEM_READ_WRITE,
					sizeof(sols_t),	NULL);

 init_failed:
  return result;
}


struct gpu_solver* gpu_solver__new(uint32_t gpu_to_use)
{
  struct gpu_solver *self = malloc(sizeof(struct gpu_solver));
  if (self != NULL) {
    gpu_solver__init(self, gpu_to_use);
  }

  return self;
}


int gpu_solver__destroy(struct gpu_solver *self)
{
  cl_int status;
  assert(CL_SUCCESS == 0);

  /* Clean up all buffers */
  if (self->dbg)
    free(self->dbg);
  clReleaseMemObject(self->buf_dbg);
  clReleaseMemObject(self->buf_ht[0]);
  clReleaseMemObject(self->buf_ht[1]);

  status = CL_SUCCESS;
  status |= clReleaseKernel(self->k_init_ht);
  for (unsigned round = 0; round < PARAM_K; round++)
    status |= clReleaseKernel(self->k_rounds[round]);
  status |= clReleaseProgram(self->program);
  status |= clReleaseCommandQueue(self->queue);
  status |= clReleaseContext(self->context);
  if (status)
    fprintf(stderr, "Cleaning resources failed\n");
  if (self->devices != NULL)
    free(self->devices);
  free(self);

  return (int)status;
}


/**
 * @return MIN(number of found valid solutions, max_solutions)
 */
size_t gpu_solver__extract_valid_solutions(struct gpu_solver *self,
					   struct gpu_solver__encoded_solution sols[],
					   unsigned int max_solutions)
{
  uint32_t counted;
  sols_t *all_sols;
  uint32_t nr_valid_sols;

  all_sols = (sols_t*)malloc(sizeof(sols_t));
  if (!all_sols)
    fatal("malloc: %s\n", strerror(errno));
  check_clEnqueueReadBuffer(self->queue, self->buf_sols,
			    CL_TRUE,	// cl_bool	blocking_read
			    0,		// size_t	offset
			    sizeof (*all_sols),	// size_t	size
			    all_sols,	// void		*ptr
			    0,		// cl_uint	num_events_in_wait_list
			    NULL,	// cl_event	*event_wait_list
			    NULL);	// cl_event	*event
  if (all_sols->nr > MAX_SOLS) {
    fprintf(stderr, "%d (probably invalid) solutions were dropped!\n",
	    all_sols->nr - MAX_SOLS);
    all_sols->nr = MAX_SOLS;
  }
  nr_valid_sols = 0;
  for (unsigned sol_i = 0; sol_i < all_sols->nr; sol_i++) {
    nr_valid_sols += verify_sol(all_sols, sol_i);
  }
  fprintf(stderr, "%d sol%s\n", nr_valid_sols, nr_valid_sols == 1 ? "" : "s");
  debug("Stats: %d likely invalids\n", all_sols->likely_invalids);

  for (uint32_t i = 0; i < all_sols->nr; i++) {
    if (all_sols->valid[i]) {
      if (counted >= nr_valid_sols)
	fatal("Bug: more than %d solutions\n", nr_valid_sols);
      gpu_solver__encode_sol(all_sols->values[i], 1 << PARAM_K,
			     sols[counted].bytes);
      counted++;
    }
    if (counted >= max_solutions) {
      debug("Cannot extract more than %d solutions\n", max_solutions);
      break;
    }
  }
  free(all_sols);
  return nr_valid_sols;
}


/**
 * @return see gpu_solver__extract_valid_solutions()
 */
unsigned int gpu_solver__find_sols(struct gpu_solver *self, uint8_t *header,
				   size_t header_len,
				   struct gpu_solver__encoded_solution sols[],
				   unsigned int max_solutions)
{
  uint64_t total = 0;

  uint64_t t0 = now();
  solve_equihash(self->context, self->queue, self->k_init_ht,
		 self->k_rounds, self->k_sols, self->buf_ht,
		 self->buf_sols, self->buf_dbg, self->dbg_size,
		 header, header_len, 0, 0, NULL, NULL, NULL, false);

  uint64_t t1 = now();
  total = gpu_solver__extract_valid_solutions(self, sols, max_solutions);

  fprintf(stderr, "Total %ld solutions in %.1f ms (%.1f Sol/s)\n",
	  total, (t1 - t0) / 1e3, total / ((t1 - t0) / 1e6));

  return (unsigned int)total;
}
