# Version 4 (08 Nov 2016)

* Add Nvidia GPU support (fix more unaligned memory accesses)
* Add nerdralph's optimization for potential +10% speedup (OPTIM_SIMPLIFY_ROUND)
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
