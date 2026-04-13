# FastRing Release Notes

## Highlights

- Improved cross-platform build portability by removing CPU-specific compiler tuning from default builds.
- Hardened runtime behavior for Windows socket initialization and POSIX sleep handling.
- Fixed ADC simulator edge cases around 32-bit resolution and callback synchronization.
- Improved ring buffer portability with standard C11 memory fences and stronger allocation checks.
- Added timestamp regression coverage and expanded buffer wrap-around tests.

## Verification

- Built in `Release` mode with CMake.
- Ran `ctest --output-on-failure` successfully.
- Produced distributable archives with CPack.

## Package Contents

- `bin/adc_acquisition` executable
- `bin/tests/` regression and performance test executables
- `include/` public headers
- `share/ADCDataAcquisition/` project license, README, and docs
