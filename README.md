# SILENTARMY

SILENTARMY is an OpenCL GPU Zcash Equihash solver. It runs best on AMD GPUs
and implements the CLI API described in the
[Zcash open source miner challenge](https://zcashminers.org/rules).

To solve a specific block header and print the encoded solution on stdout, run
the following command (this header is from
[testnet block #2680](https://explorer.testnet.z.cash/block/0000045e1a6af7b9017190297177807f98d60144b5aa525b6ae152c2ddc64966)
and should result in 3 solutions):

    $ silentarmy -i 0400000052a6a17bb3cf95c62ec140d22f4fe96cfbc192ff288251282174481312040000b9711b4850b4b89598e16103148a8a368f74e472fa919ac7d0dbb57b1090f6c80000000000000000000000000000000000000000000000000000000000000000667211581e1b071e4302000000000000020000000000000000000000000000000000000000000000

If the option `-i` is not specified, SILENTARMY solves a 140-byte header of all
zero bytes. The option `--nonces <nr>` instructs SILENTARMY to try multiple
nonces, each time incrementing the nonce by 1. So a convenient way to run a
benchmark is simply:

    $ silentarmy --nonces 100

Note: due to BLAKE2b optimizations in my implementation, if the header is
specified it must be 140 bytes and its last 12 bytes **must** be zero. For
convenience, `-i` can also specify a 108-byte nonceless header to which
SILENTARMY adds an implicit nonce of 32 zero bytes.

Use the verbose (`-v`) and very verbose (`-v -v`) options to show the solutions
and statistics in progressively more and more details.

# Performance

* 45.7 Sol/s with one R9 Nano
* 39.6 Sol/s with one RX 480 8GB

Note: run 2 instances of SILENTARMY in parallel (eg. in 2 terminal consoles)
on the same GPU to reach these performance numbers. The code is currently very
poorly optimized; it makes zero attempts to keep the queue of OpenCL commands
full, therefore 2 instances are needed to keep the GPU fully utilized.

# Dependencies

SILENTARMY has primarily been tested with AMD GPUs on 64-bit Linux with the
**AMDGPU-PRO** driver (amdgpu.ko) or the **Radeon Software Crimson Edition**
driver (fglrx.ko). Its only build dependency is the OpenCL C headers from the
**AMD APP SDK**.

Installation of the drivers and SDK can be error-prone, so below are
step-by-step instructions for Ubuntu 16.04 as well as Ubuntu 14.04.

## Ubuntu 16.04

1. Download the [AMDGPU-PRO Driver](http://support.amd.com/en-us/kb-articles/Pages/AMDGPU-PRO-Install.aspx)
(as of 30 Oct 2016, the latest version is 16.40)

2. Extract it:
   `$ tar xf amdgpu-pro-16.40-348864.tar.xz`

3. Install (non-root, will use sudo access automatically):
   `$ ./amdgpu-pro-install`

4. Add yourself to the video group if not already a member:
   `$ sudo gpasswd -a $(whoami) video`

5. Reboot

6. Download the [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/)
(as of 27 Oct 2016, the latest version is 3.0)

7. Extract it:
   `$ tar xf AMD-APP-SDKInstaller-v3.0.130.136-GA-linux64.tar.bz2`

8. Install system-wide by running as root (accept all the default options):
  `$ sudo ./AMD-APP-SDK-v3.0.130.136-GA-linux64.sh`

9. Install a compiler tools which you will need to compile SILENTARMY:
  `$ sudo apt-get install build-essential`

## Ubuntu 14.04

1. Install the official Ubuntu package:
   `$ sudo apt-get install fglrx`
   (as of 30 Oct 2016, the latest version is 2:15.201-0ubuntu0.14.04.1)

2. Follow steps 5-9 above.

# Compilation and installation

Compiling SILENTARMY is easy:

`$ make`

You may need to edit the `Makefile` and change the path
`/opt/AMDAPPSDK-3.0/include` if **AMD APP SDK** was installed in a non-default
location. Also if you are not using the **AMDGPU-PRO Driver** you may need
to edit the `Makefile` and change the path to `libOpenCL.so`.

Self-testing (solves 100 all-zero 140-byte blocks with their nonces varying
from 0 to 99):

`$ make test`

For more testing run `silentarmy --nonces 10000`. It should finds 18681
solutions which is less than 1% off the theoretical expected average number of
solutions of 1.88 per Equihash run at (n,k)=(200,9).

For installing, just copy `silentarmy` wherever.

# Implementation details

SILENTARMY uses two hash tables to avoid having to sort the (Xi,i) pairs:

* Round 0 (BLAKE2b) fills up table #0
* Round 1 reads table #0, identifies collisions, XORs the Xi's, stores
  the results in table #1
* Round 2 reads table #1 and fills up table #0 (reusing it)
* Round 3 reads table #0 and fills up table #1 (also reusing it)
* ...
* Round 8 (last round) reads table #1 and fills up table #0.

Only the non-zero parts of Xi are stored in the hash table, so fewer and fewer
bytes are needed to store Xi as we progress toward round 8. For a description
of the layout of the hash table, see the comment at the top of `input.cl`.

Also the code implements the notion of "encoded reference to inputs" which
I--apparently like most authors of Equihash solvers--independently discovered
as a neat trick to save having to read/write so much data. Instead of saving
lists of inputs that double in size every round, SILENTARMY re-uses the fact
they were stored in the previous hash table, and saves a reference to the two
previous inputs, encoded as a (row,slot0,slot1) where (row,slot0) and
(row,slot1) themselves are each a reference to 2 previous inputs, and so on,
until round 0 where the inputs are just the 21-bit values.

A BLAKE2b optimization implemented by SILENTARMY requires the last 12 bytes of
the nonce/header to be zero. When set to a fixed value like zero, not only the
code does not need to implement the "sigma" permutations, but many 64-bit
additions in the BLAKE2b mix() function can be pre-computed automatically by
the OpenCL compiler.

Managing invalid solutions (duplicate inputs) is done in multiple places:

* Any time a XOR results in an all-zero value, this work item is discarded
as it is statistically very unlikely that the XOR of 256 or fewer inputs
is zero. This check is implemented at the end of `xor_and_store()`
* When the final hash table produced at round 8 has many elements
that collide in the same row (because bits 160-179 are identical, and
almost certainly bits 180-199), this is also discarded as a likely invalid
solution because this is statistically guaranteed to be all inputs repeated
at least once. This check is implemented in `kernel_sols()` (see
`likely_invalids`.)
* Finally when the GPU returns potential solutions, the CPU also checks for
invalid solutions with duplicate inputs. This check is implemented in
`verify_sol()`.

Finally, SILENTARMY makes many optimization assumptions and currently only
supports Equihash parameters 200,9.

# Author

Marc Bevand -- [http://zorinaq.com](http://zorinaq.com)

# License

The MIT License (MIT)
Copyright (c) 2016 Marc Bevand

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
