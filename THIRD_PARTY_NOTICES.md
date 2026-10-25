# Sources and attribution

This is a board-specific derivative, not an independent replacement for the
upstream unlock research. Original copyright and license headers are retained.

- Zagitta/gpu-unlock, main 4189b17711bd487d9ea4a8ff0055e6d58d19d6d3:
  https://github.com/Zagitta/gpu-unlock
- elias-gaeros/gpu-unlock, rtx4090 4ec490d49c1cc292c0b4cd5ce5a801b5d2c396dc:
  https://github.com/elias-gaeros/gpu-unlock/tree/rtx4090
  The upstream-4090 directory contains the kernel helper/test source subset used
  for native regression tests, with its GPL-2.0-only license.
- TOTORONG/AD103_win_unlocker, 51df9c17ec89eb8486df52cc9079dde4bd1f9974:
  https://huggingface.co/TOTORONG/AD103_win_unlocker
  This supplies the inherited Windows UEFI code, compatibility layer, firmware
  tooling and test scaffolding. The port and derived modifications remain GPL.
  Its original narrative documentation is not bundled in this repository.
- TianoCore EDK2 headers, c1415771ac592ad8509351c46cd3cda4f38d7d52:
  https://github.com/tianocore/edk2
  BSD-2-Clause-Patent; see port/source/LICENSES/EDK2-BSD-2-Clause-Patent.txt.
- The inherited vendor core traces to Zagitta/gpu-unlock
  0ebdb1c6308b6a2072be5bf04513472942541956 (GPL-2.0-only).
- WildFlash1st/cmp90hx-unlock V67 reference,
  76940ae583281daa2b883518a2e89b56c11d3a46 (GPL version 2):
  https://github.com/WildFlash1st/cmp90hx-unlock
  This is a CMP90HX reference. Its existence does not establish support for
  arbitrary GeForce boards or drivers.

Our changes adapt the target/profile to this PNY AD102 board, constrain cold
selectors, pin ROM and firmware hashes, preload a measured Windows ESP, restore
GOP through a saved output path and SetMode, and add automatic USB countdown
entry with cancellation. They also add board, failure-injection, display and
countdown tests plus a separate read-only diagnostic.

Firmware extraction helpers retain GPL headers. Proprietary firmware is fetched
and generated locally, with input hashes checked. NVIDIA license text is retained
in port/source/LICENSES/NVIDIA-Driver.txt. No firmware redistribution permission
is asserted.
