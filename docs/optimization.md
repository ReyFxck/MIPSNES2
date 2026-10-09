# Optimization process and log

No performance optimization exists yet. No attempts have been retained or rejected.
PS2 and PSP must be profiled independently. PCSX2/PPSSPP results never substitute for
real hardware. Correct reference behavior precedes optimized implementations.

- [ ] Correct C implementation
- [ ] Tests
- [ ] Profiling
- [ ] Hotspot identified
- [ ] Baseline benchmark
- [ ] Optimized C attempted where reasonable
- [ ] MIPS implementation considered
- [ ] MIPS implementation written if justified
- [ ] Correctness regression tests
- [ ] New benchmark
- [ ] Documentation updated

Use PERFORMANCE.md's measurement protocol. Evaluate allocation, memory traffic,
layout, branches, fast/slow paths and synchronization before assuming assembly or
vector units help. Aim for zero per-frame dynamic allocations through bounded
preallocation. Keep interfaces explicit about aliasing, alignment, format and length.

## Per-attempt record

```text
Optimization:
Target:
Subsystem:
Reason:
Baseline:
Result:
Correctness tests:
Complexity cost:
Keep/Revert:
Notes:
```

Add revision/toolchain, exact workload, real-device identity, raw measurement link,
reference/optimized outputs and rejected alternatives. Failed attempts are useful
project memory. Preserve working MIPS/MMI/VU/VFPU/DMA/layout code unless measurement
and regression evidence justify replacement. Approximation records also require
compatibility impact and a refinement trigger. No "should be faster" conclusions.
