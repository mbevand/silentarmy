# Troubleshooting

Follow this checklist to verify that your entire hardware and software
stack works (drivers, OpenCL, SILENTARMY).

## Driver / OpenCL installation

Run `clinfo` to list all the OpenCL devices. If it does not find all your
devices, something is wrong with your drivers and/or OpenCL stack. Uninstall
and reinstall your drivers. Here are good instructions:
https://hashcat.net/wiki/doku.php?id=frequently_asked_questions#i_may_have_the_wrong_driver_installed_what_should_i_do

## Check silentarmy

Does `./silentarmy --list` list your devices? If `clinfo` does, silentarmy
should list them as well.

## Basic operation 

Run the Equihash solver `sa-solver` to solve the all-zero block. It should
report 2 solutions. Specify the device ID to test with `--use ID`

```
$ ./sa-solver --use 0
Solving default all-zero 140-byte header
Building program
Hash tables will use 805.3 MB
Running...
Nonce 0000000000000000000000000000000000000000000000000000000000000000: 2 sols
Total 2 solutions in 205.3 ms (9.7 Sol/s)
```

Note that `sa-solver` only supports 1 device at a time. It will not recognize
eg. `--use 0,1,2`.

## Correct results

Verify that `make test` reports valid Equihash solutions for 100 different
blocks:

```
$ make test
./sa-solver --nonces 100 -v -v 2>&1 | grep Soln: | \
    diff -u testing/sols-100 - | cut -c 1-75
```

It should output nothing else. If you see a bunch of lines with numbers,
something is wrong with your hardware and/or drivers.

## Sustained operation on one device

Let the Equihash solver `sa-solver` run for multiple hours:

```
$ ./sa-solver --nonces 100000000
Solving default all-zero 140-byte header
Building program
Hash tables will use 1208.0 MB
Running...
Nonce 0000000000000000000000000000000000000000000000000000000000000000: 2 sols
Nonce 0100000000000000000000000000000000000000000000000000000000000000: 0 sols
...
```

It should not crash or hang.

## Mining

Run the miner without options. By default it will use the first device,
and connect to flypool with my donation address. These known-good parameters
should let you know easily if your machine can mine properly:

```
$ ./silentarmy
Connecting to us1-zcash.flypool.org:3333
Stratum server sent us the first job
Mining on 1 device
Total 0.0 sol/s [dev0 0.0] 0 shares
Total 48.9 sol/s [dev0 48.9] 1 share
Total 44.9 sol/s [dev0 44.9] 1 share
...
```

Verify that the number of shares increases over time.

## Performance

Not achieving the performance you expected?

* By default SILENTARMY mines with only one device/GPU; make sure to specify
  all the GPUs in the `--use` option, for example `silentarmy --use 0,1,2`
  if the host has three devices with IDs 0, 1, and 2.
* If a GPU has less than 2 GB of GPU memory, run `silentarmy --instances 1`
  (1 instance uses ~0.8 GB of memory, 2 instances use ~1.6 GB of memory.)
* If 1 instance still requires too much memory, edit `param.h` and set
  `NR_ROWS_LOG` to `19` (this reduces the per-instance memory usage to ~670 MB)
  and run with `--instances 1`.
