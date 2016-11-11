from cffi import FFI
import os.path
import inspect

ffi = None
library = None

library_header = """
struct gpu_solver__encoded_solution {
  uint8_t bytes[1344];
};

struct solution {
  unsigned int data[512];
};


struct gpu_solver;

struct gpu_solver* gpu_solver__new(uint32_t platform_id, uint32_t gpu_id, bool verbose);
int gpu_solver__destroy(struct gpu_solver *self);

unsigned int gpu_solver__find_sols(struct gpu_solver *self, uint8_t *header,
                                   size_t header_len,
                                   struct gpu_solver__encoded_solution sols[],
                                   unsigned int max_solutions);
"""

def load_library():
    global library, ffi
    assert library is None

    ffi = FFI()
    ffi.cdef(library_header)

    library = ffi.dlopen('libsilentarmy.so')
    assert library is not None


class Solver(object):
    max_solutions = 16

    def __init__(self, gpu_id, verbose=True):
        self.solver_ = self.header_ = self.solutions_ = self.solution_to_check_ = None
        self._ensure_library()
        assert library and ffi
        self.solver_ = library.gpu_solver__new(gpu_id[0], gpu_id[1], verbose)
        self.solutions_ = ffi.new('struct gpu_solver__encoded_solution[%s]' % self.max_solutions)

    def __del__(self):
        # Free the underlying resources on destruction
        if (self.solver_ is not None):
            library.gpu_solver__destroy(self.solver_);
            self.solver_ = None
            self.header_ = self.solutions_ = None

    def _ensure_library(self):
        # Try to load library from standard
        if library is None:
            load_library()

    def find_solutions(self, block_header):
        """
        @return a number of found solutions
        """
        block_header_len = len(block_header)
        assert block_header_len == 140
        return library.gpu_solver__find_sols(self.solver_,
                                             ffi.cast('uint8_t*', ffi.from_buffer(block_header)),
                                             block_header_len,
                                             self.solutions_,
                                             self.max_solutions)

    def get_solution(self, num):
        assert(num >= 0 and num < self.max_solutions)
        return bytes(ffi.buffer(self.solutions_[num].bytes))
