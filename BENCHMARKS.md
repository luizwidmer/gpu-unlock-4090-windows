# Before and after: tested PNY RTX 4090

Matched local workloads, driver 617.14, manual R4 unlock, 2026-10-24.
Values below are median seconds; lower is faster.

| Workload | Before | After | Speedup |
|---|---:|---:|---:|
| Qwen speech, warm | 19.38 | 12.88 | 1.51x |
| ACE music | 46.43 | 33.25 | 1.40x |
| MOSS, 8 steps + two LoRAs | 62.42 | 43.28 | 1.44x |
| MOSS, 50 steps | 98.57 | 82.36 | 1.20x |
| H3 video | 54.42 | 48.32 | 1.13x |
| YuE music | 45.54 | 34.38 | 1.32x |
| Music analysis | 57.40 | 45.34 | 1.27x |
| EDMFormer | 14.38 | 12.24 | 1.17x |
| LTX video, warm, uncached | 35.50 | 30.72 | 1.16x |

Native FP16 with FP32 accumulation: 136.3 -> 320.2 TFLOP/s (2.35x).
BF16 with FP32 accumulation: 135.6 -> 318.6 TFLOP/s (2.35x).
Instruction measurements used 40,000 iterations and five samples.

All nine numerical GEMM cases passed finite-output and CPU float64-reference
checks. There were 29 independently verified usable API trials. One H3 metadata
file failure was excluded and replaced with an identical trial; it was preserved
in the private evidence. Cached LTX outputs were not used for these medians.

These are measurements on one board across Windows boots, with automatic clocks,
thermal variation and OS/compiler caches. They do not isolate every gain as a
causal unlock effect. Direct post-driver register readback was unavailable;
functional instruction throughput was checked before and after the API suite.
Private prompts, generated media, logs, captures and system identifiers are not
published. These results describe manual R4, not hardware validation of R5 auto.
