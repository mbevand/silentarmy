# Current tip

* Avoid 100% CPU usage with Nvidia's OpenCL, aka busywait fix (Kubuxu)
* Optimization: +10% speedup, increase collision items tracked per thread
  (nerdralph). 'make test' finds 196 sols again.
* Implement mining.extranonce.subscribe (kenshirothefist)

# Version 5 (11 Nov 2016)

* Optimization: major 2x speedup (eXtremal) by storing 8 atomic counters in
  1 uint, and by reducing branch divergence when iterating over and XORing Xi's.
  Note that as a result of these optimizations, sa-solver compiled with
  NR_ROWS_LOG=20 now only finds 182 out of 196 existing solutions ("make test"
  verification data was adjusted accordingly)
* Defaulting OPTIM_SIMPLIFY_ROUND to 1; GPU memory usage down to 0.8 GB per
  instance
* Optimization: significantly reduce CPU usage and PCIe bandwidth (before:
  ~100 MB/s/GPU, after: 0.5 MB/s/GPU), accomplished by filtering invalid
  solutions on-device
* Optimization: reduce size of collisions[] array; +7% speed increase measured
  on RX 480 and R9 Nano using AMDGPU-PRO 16.40
* Implement stratum method client.reconnect
* Avoid segfault when encountering an out-of-range input
* For simplicity `-i <header>` now only accepts 140-byte headers
* Update README.md with Nvidia performance numbers
* Fix mining on Xeon Phi and CPUs (fix OpenCL warnings)
* Fix compilation warnings and 32-bit platforms

# Version 4 (08 Nov 2016)

* Add Nvidia GPU support (fix more unaligned memory accesses)
* Add nerdralph's optimization (OPTIM_SIMPLIFY_ROUND) for potential +30%
  speedup, especially useful on Nvidia GPUs
* Drop the Python 3.5 dependency; now requires only Python 3.3 or above (lhl)
* Drop the libsodium dependency; instead use our own SHA256 implementation
* Add nicehash compatibility (stratum servers fixing 17 bytes of the nonce)
* Only apply set_target to *next* mining job
* Do not abandon previous mining jobs if clean_jobs is false
* Fix KeyError's when displaying stats
* Be more robust about different types of network errors during connection
* Remove bytes.hex() which was only supported on Python 3.5+.

# Version 3 (04 Nov 2016)

* SILENTARMY is now a full miner, not just a solver; the solver binary was
  renamed "sa-solver" and the miner is the script "silentarmy"
* Multi-GPU support
* Stratum support for pool mining
* Reduce GPU memory usage to 671 MB (NR_ROWS_LOG=19) or 1208 MB
  (NR_ROWS_LOG=20, default, ~10% faster than 19) per Equihash instance
* Rename --list-gpu to --list and list all OpenCL devices (not just GPUs)
* Add support for multiple OpenCL platforms: --list now scans all available
  platforms, numbering devices using globally unique IDs
* Improve correctness: find ~0.09% more solutions

# Version 2 (30 Oct 2016)

* Support GCN 1.0 / remove unaligned memory accesses (because of this bug,
  previously SILENTARMY always reported 0 solutions on GCN 1.0 hardware)
* Minor performance improvement (~1%)
* Get rid of "kernel.cl" and move the OpenCL code to a C string embedded in the
  binary during compilation
* Update README with instructions for installing
  **Radeon Software Crimson Edition** (fglrx.ko) in addition to
  **AMDGPU-PRO** (amdgpu.ko)

# Version 1 (27 Oct 2016)

* Initial import into GitHub
