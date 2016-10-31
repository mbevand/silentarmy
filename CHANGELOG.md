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
