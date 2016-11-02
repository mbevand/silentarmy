# Current

* Reduce GPU memory usage to 671 MB (NR_ROWS_LOG=19) or 1208 MB
  (NR_ROWS_LOG=20, default, ~10% faster than 19) per SILENTARMY instance
* Add support for multiple OpenCL platforms: --list-gpu now scans all available
  platforms, numbering devices using globally unique IDs.
* Improve correctness: find ~0.09% more solutions

# Version 2 (30 Oct 2016)

* Support GCN 1.0 / remove unaligned memory accesses (because of this bug,
  previously SILENTARMY always reported 0 solutions on GCN 1.0 hardware)
* Minor performance improvement (~1%)
* Get rid of "kernel.cl" and move the OpenCL code to a C string embedded in the
  binary during compilation.
* Update README with instructions for installing
  **Radeon Software Crimson Edition** (fglrx.ko) in addition to
  **AMDGPU-PRO** (amdgpu.ko).

# Version 1 (27 Oct 2016)

* Initial import into GitHub
