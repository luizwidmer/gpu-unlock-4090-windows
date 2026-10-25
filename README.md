# RTX 4090 Windows UEFI unlock port

A PNY RTX 4090 / AD102 board-specific Windows handoff built on the gpu-unlock
research and TOTORONG UEFI port. Our original tooling and docs are MIT;
inherited code keeps its licenses. See [licensing](LICENSING.md) and
[upstream credits](THIRD_PARTY_NOTICES.md).

## What this adds

- Measured PNY board, ROM hash and cold-selector gates; no generic 4090 support claim.
- One SEC2 transaction with checked cleanup and strict post-unlock selectors.
- Saved-output GOP reconnection and SetMode before Windows handoff.
- Manual U entry or a separate automatic USB entry with a five-second Esc window.
- Read-only EFI capture and native host simulations for failures and recovery.

The default build returns ACCESS_DENIED before accessing EFI services or GPU
hardware. Enabled builds require separate packaging, private measured inputs and
an explicit local ESP configuration. Nothing in the normal build or test commands
changes boot settings, runs an unlock, flashes VBIOS or stages a USB.

## Build and test on Windows x64

Install Python 3.14+ and Zig 0.14.1. Set ZIG_EXE to the absolute path to zig.exe,
or unpack Zig into tools/zig-x86_64-windows-0.14.1/. No Python packages are needed.

```powershell
$env:ZIG_EXE = 'C:\tools\zig-0.14.1\zig.exe'
python test.py
python build.py
python diagnostic/build_probe.py
```

The default host tests use synthetic data. Exact-firmware transaction cases run
only when all local firmware assets exist; ROM-dependent groups run only when
inputs/PNY-RTX4090.rom exists. Test metadata records which cases ran.
No firmware binary or private capture is bundled.
The independent diagnostic builder runs 25 synthetic tests and builds a read-only
probe; it does not link the unlocker.

For the full private-fixture suite, first generate the pinned firmware locally:

```powershell
python prepare_assets.py
python test_board.py
python test_auto_boot.py
python diagnostic/test_verify_capture.py
```

These require the matching exported ROM and the verified board capture under
inputs/captures/20261024-CAP00/. Missing or mismatched evidence must block these
tests and enabled packaging. Original local results: 171 board scenarios,
23 automatic scenarios, 11 preparation groups and 10 capture-review cases.

## Enabled USB builds

Read [BOOT.md](BOOT.md) first. The published ESP GUID is deliberately unconfigured;
create ignored port/source/unlock/local_board.h using measured ESP GUID,
partition number, start LBA and length. ROM/board pins remain specific to the
original tested board and must not be loosened to force another board through.

```powershell
python prepare_soft_unlock.py --revision manual-local
python prepare_soft_unlock.py --revision auto-local --automatic
```

Packaging reruns required tests and produces a local usb-package. It does not
stage or execute it. Machine-specific USB staging scripts are intentionally
excluded. The read-only diagnostic has a separate guarded Stage-Usb.ps1.

## Hardware validation and performance

The manual R4 path completed unlock, display recovery and Windows handoff on the
tested PNY board with VBIOS 95.02.18.C0.09 and driver 617.14. The automatic R5
variant passed simulations but had not run on hardware at publication.
Windows functional throughput tests confirmed the lifted cap; direct Windows
register readback was unavailable. See [before/after results](BENCHMARKS.md).

This experimental preboot path can leave the display black or require physical
power-off on failure. It is board-specific and has no automatic retry.
