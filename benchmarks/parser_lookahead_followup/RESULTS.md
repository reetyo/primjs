# PrimJS parser lookahead follow-up results

Each independently generated JavaScript input is about 2.1 MB. The table
reports paired median savings when both follow-up optimizations are enabled.
Positive values indicate lower elapsed time. Asterisks mark intervals that
include zero. All quantities are in milliseconds.

| Workload | macOS compile | macOS source eval | Android compile | Android source eval |
| --- | ---: | ---: | ---: | ---: |
| Function and arrow wrappers | 14.18 | 14.37 | 12.05 | 11.84 |
| Mixed expressions | 7.40 | 7.56 | 8.53 | 7.95 |
| Object with token fallback | 0.16* | -0.05* | 0.95* | 0.71 |

The wrapper prefix check accounts for most of the measured savings. The
confidence intervals for token kinds alone include zero on all three
inputs and both platforms. These measurements describe the generated
workloads and the engine timing region.

The macOS run used an ARM64 Apple M5 Pro with macOS 26.7.1. The Android run
used an Android 15 ARM64 emulator on the same machine. See the raw CSV
files and summary files for all four configurations and their intervals.

Both tracing-GC Test262 comparisons covered 39,614 cases per configuration.
All four runs reported the same 1,289 existing failures by test path.
They also each reported 1,314 exclusions and 5,718 skips. All four
configurations passed the added syntax cases on macOS GC, macOS RC, and
Android GC.

[Reproduction commands](README.md)
