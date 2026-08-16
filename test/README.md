# Test code

This is test/debug code for the energy P1 level II driver

The test executable loads the driver's JSON configuration and starts its normal
worker thread with a captured P1 telegram file instead of the configured serial
port. The worker validates each frame and calls `doWork`; matched measurements
are then written in a compact form to standard output.

```sh
./test ../../debug/linux/energyp1.json ../../python/hanp1_1.data
```
