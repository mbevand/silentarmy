# SILENTARMY Standalone Version for Windows x86_64

[Download precompiled binaries (v5-win64standalone-r9)](https://github.com/zawawawa/silentarmy/releases/download/v5-win64standalone-r9/silentarmy-v5-win64standalone-r9.zip) (Developmental)

[Download precompiled binaries (v5-win64standalone-r7)](https://github.com/zawawawa/silentarmy/releases/download/v5-win64standalone-r7/silentarmy-v5-win64standalone-r7.zip) (Stable)

This is a standalone Windows x86_64 port of SILENTARMY v5, which does not require Python, based on [Genoil's Windows port](https://github.com/Genoil/silentarmy/tree/windows). All you have to do for mining is to run `list.bat` to get device ID's and edit and run `silentarmy.bat`. MAKE SURE TO SPECIFY CORRECT DEVICE IDS WITH THE `--use` OPTION! See the documentation of the original SILENTARMY below for details.

You may get the following error if you have missing DLL's on your system: `The application was unable to start correctly (0xc000007b)` In this case, you need to delete `vcruntime140.dll` in the package and install [Visual C++ 2015 Redistribution Package](https://www.microsoft.com/en-us/download/details.aspx?id=48145).

If you find this port useful and/or would like to see a feature-rich ZEC miner based on it, please consider donations to: `t1NwUDeSKu4BxkD58mtEYKDjzw5toiLfmCu`

Last but not least, mrb, nerdralph, eXtremal, and Genoil, thank you all so much for the great work. You guys are 
truly the cream of the FOSS movement.

zawawa @ bitcointalk.org

# SILENTARMY

Official site: https://github.com/mbevand/silentarmy

SILENTARMY is a free open source [Zcash](https://z.cash) miner for Linux
with multi-GPU and [Stratum](https://github.com/str4d/zips/blob/77-zip-stratum/drafts/str4d-stratum/draft1.rst) support. It is written in OpenCL and has been tested
on AMD/Nvidia/Intel GPUs, Xeon Phi, and more.

After compiling SILENTARMY, list the available OpenCL devices:

```
$ silentarmy --list
```

Start mining with two GPUs (ID 2 and ID 5) on a pool:

```
$ silentarmy --use 2,5 -c stratum+tcp://us1-zcash.flypool.org:3333 -u t1cVviFvgJinQ4w3C2m2CfRxgP5DnHYaoFC
```

When run without options, SILENTARMY mines with the first OpenCL device, using
my donation address, on flypool:

```
$ silentarmy
Connecting to us1-zcash.flypool.org:3333
Stratum server sent us the first job
Mining on 1 device
Total 0.0 sol/s [dev0 0.0] 0 shares
Total 43.9 sol/s [dev0 43.9] 0 shares
Total 46.9 sol/s [dev0 46.9] 0 shares
Total 44.9 sol/s [dev0 44.9] 1 share
[...]
```

Usage:

```
$ silentarmy --help
Usage: silentarmy [options]

Options:
  -h, --help            show this help message and exit
  -v, --verbose         verbose mode (may be repeated for more verbosity)
  --debug               enable debug mode (for developers only)
  --list                list available OpenCL devices by ID (GPUs...)
  --use=LIST            use specified GPU device IDs to mine, for example to
                        use the first three: 0,1,2 (default: 0)
  --instances=N         run N instances of Equihash per GPU (default: 2)
  -c POOL, --connect=POOL
                        connect to POOL, for example:
                        stratum+tcp://example.com:1234
  -u USER, --user=USER  username for connecting to the pool
  -p PWD, --pwd=PWD     password for connecting to the pool
```

# Performance

* 115 sol/s with one R9 Nano
* 75 sol/s with one RX 480 8GB
* 70 sol/s with one GTX 1070

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md#performance) to resolve performance
issues.

Note: the `silentarmy` **miner** automatically achieves this performance level,
however the `sa-solver` **command-line solver** by design runs only 1 instance
of the Equihash proof-of-work algorithm causing it to slightly underperform by
5-10%. One must manually run 2 instances of `sa-solver` (eg. in 2 terminal
consoles) to achieve the same performance level as the `silentarmy` **miner**.

# Compilation and installation

The steps below describe how to obtain the dependencies needed by SILENTARMY,
how to compile it, and how to install it.

## Step 1: OpenCL

OpenCL support comes with the graphic card driver. Read the appropriate
subsection below:

### Ubuntu 16.04 / amdgpu

1. Download the [AMDGPU-PRO Driver](http://support.amd.com/en-us/kb-articles/Pages/AMDGPU-PRO-Install.aspx)
(as of 30 Oct 2016, the latest version is 16.40).

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

### Ubuntu 14.04 / fglrx

1. Install the official Ubuntu package for the **Radeon Software Crimson
   Edition** driver:
   `$ sudo apt-get install fglrx`
   (as of 30 Oct 2016, the latest version is 2:15.201-0ubuntu0.14.04.1)

2. Follow steps 5-8 above: reboot, install the AMD APP SDK...

### Ubuntu 16.04 / Nvidia

1. Install the OpenCL development files and the latest driver:
   `$ sudo apt-get install nvidia-opencl-dev nvidia-361`

2. Either reboot, or load the kernel driver:
   `$ sudo modprobe nvidia_361`

## Step 2: Python 3.3

1. SILENTARMY requires Python 3.3 or later (needed to support the use of the
   `yield from` syntax). On Ubuntu/Debian systems:
   `$ sudo apt-get install python3`

2. Verify the Python version is 3.3 or later:
   `$ python3 -V`

## Step 3: C compiler

1. A C compiler is needed to compile the SILENTARMY solver binary (`sa-solver`):
   `$ sudo apt-get install build-essential`

## Step 4: Get SILENTARMY

Download it as a ZIP from github: https://github.com/mbevand/silentarmy/archive/master.zip

Or clone it from the command line:
`$ git clone https://github.com/mbevand/silentarmy.git`

Or, for Arch Linux users, get the
[silentarmy AUR package](https://aur.archlinux.org/packages/silentarmy/).

## Step 5: Compile and install

Compiling SILENTARMY is easy:

`$ make`

You may need to specify the paths to the locations of your OpenCL C headers
and libOpenCL.so if the compiler does not find them, eg.:

`$ make OPENCL_HEADERS=/usr/local/cuda-8.0/targets/x86_64-linux/include LIBOPENCL=/usr/local/cuda-8.0/targets/x86_64-linux/lib`

Self-testing the command-line solver (solves 100 all-zero 140-byte blocks with
their nonces varying from 0 to 99):

`$ make test`

For more testing run `sa-solver --nonces 10000`. It should finds 18627
solutions which is less than 1% off the theoretical expected average number of
solutions of 1.88 per Equihash run at (n,k)=(200,9).

For installing, just copy `silentarmy` and `sa-solver` to the same directory.

# Equihash solver

SILENTARMY also provides a command line Equihash solver (`sa-solver`)
implementing the CLI API described in the
[Zcash open source miner challenge](https://zcashminers.org/rules).
To solve a specific block header and print the encoded solution on stdout, run
the following command (this header is from
[mainnet block #3400](https://explorer.zcha.in/blocks/00000001687e89e7e1ce48b349e601c89c70dd4c268fdf24b269a3ca4140426f)
and should result in 1 Equihash solution):

```
$ sa-solver -i 04000000e54c27544050668f272ec3b460e1cde745c6b21239a81dae637fde4704000000844bc0c55696ef9920eeda11c1eb41b0c2e7324b46cc2e7aa0c2aa7736448d7a000000000000000000000000000000000000000000000000000000000000000068241a587e7e061d250e000000000000010000000000000000000000000000000000000000000000
```

If the option `-i` is not specified, `sa-solver` solves a 140-byte header of all
zero bytes. The option `--nonces <nr>` instructs the program to try multiple
nonces, each time incrementing the nonce by 1. So a convenient way to run a
quick test/benchmark is simply:

`$ sa-solver --nonces 100`

Note: due to BLAKE2b optimizations in my implementation, if the header is
specified it must be 140 bytes and its last 12 bytes **must** be zero.

Use the verbose (`-v`) and very verbose (`-v -v`) options to show the solutions
and statistics in progressively more and more details.

# Implementation details

The `silentarmy` Python script is actually mostly a lightweight Stratum
implementation which launches in the background one or more instances of
`sa-solver --mining` per GPU. This "mining mode" enables `sa-solver` to
communicate with `silentarmy` using stdin/stdout. By default 2 instances of
`sa-solver` are launched for each GPU (this can be changed with the `silentarmy
--instances N` option.) 2 instances per GPU usually results in the best
performance.

The `sa-solver` binary invokes the OpenCL kernel which contains the core of the
Equihash algorithm. My implementation uses two hash tables to avoid having to
sort the (Xi,i) pairs:

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
* When input references are expanded on-GPU by `expand_refs()`, the code
checks if the last (512th) input is repeated at least once.
* Finally when the GPU returns potential solutions, the CPU also checks for
invalid solutions with duplicate inputs. This check is implemented in
`verify_sol()`.

Finally, SILENTARMY makes many optimization assumptions and currently only
supports Equihash parameters 200,9.

# Author

Marc Bevand -- [http://zorinaq.com](http://zorinaq.com)

Donations welcome: t1cVviFvgJinQ4w3C2m2CfRxgP5DnHYaoFC

# Thanks

I would like to thank these persons for their contributions to SILENTARMY,
in alphabetical order:
* eXtremal
* kenshirothefist
* lhl
* nerdralph
* poiuty
* solardiz

# License

The MIT License (MIT)
Copyright (c) 2016 Marc Bevand

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
