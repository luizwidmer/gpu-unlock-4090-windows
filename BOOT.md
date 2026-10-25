# USB behavior and recovery

A manual build requires U after preflight. An automatic build uses the same
checks and waits five seconds; Esc cancels to firmware. Working firmware
keyboard input is required. Successful unlock restores the saved physical GOP
output and starts the privately pinned Windows Boot Manager.

Boot selection is external to the application. No script in this repository
sets boot order, disables DMA protections, installs a Windows driver or flashes
VBIOS. Automatic entry does not guarantee the firmware selects USB on each boot.
Warm or already-unlocked states can be rejected before an attempt.

Before replacing a USB loader, preserve its previous working manual loader,
instructions, diagnostic, assets and captures. The automatic log namespace is
RTX4090-PREP/AUTO0000 through AUTO9999; existing logs are not overwritten.

On STOP, or a black screen lasting two minutes, hold the physical power button
to switch off. Remove the USB and select Windows Boot Manager or a preserved
manual/diagnostic loader. Do not continue to Windows after a stopped transaction.
Keep the USB connected during the attempt; no automatic retry occurs.

Private local_board.h must define GU_PROFILE_WINDOWS_ESP_GUID,
GU_PROFILE_WINDOWS_ESP_PARTITION, GU_PROFILE_WINDOWS_ESP_START_LBA and
GU_PROFILE_WINDOWS_ESP_SIZE_LBA for the measured GPT ESP. Its contents must be
reviewed together with the capture, ROM and asset pins. It is deliberately
ignored and never supplied as a universal working profile.
