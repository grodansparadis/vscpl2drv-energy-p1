# Test code

This is test/debug code for the energy P1 level II driver

The test executable loads the driver's JSON configuration and starts its normal
worker thread with a captured P1 telegram file instead of the configured serial
port. The worker validates each frame and calls `doWork`; matched measurements
are then written in a compact form to standard output. Every occurrence is
printed, including repeated items from later frames. CRC failures are reported
on standard error and cause exit status 2. Each valid telegram starts with a
numbered `--- telegram N ---` separator. The final summary includes the worker
parsing time in milliseconds.

```sh
./test ../../debug/linux/energyp1.json ../../python/hanp1_1.data
```

```sh
./test ../../debug/linux/energyp1.json ../../python/hanp1_1-crc-fault.data
```

