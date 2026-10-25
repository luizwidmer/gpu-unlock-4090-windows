# SPDX-License-Identifier: MIT
# Read-only inventory. No GPU resets, driver changes, EFI writes or boot commands.
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$gpuText = & nvidia-smi --query-gpu=name,pci.bus_id,pci.device_id,pci.sub_device_id,vbios_version,driver_version,memory.total,memory.reserved,display_active --format=csv,noheader,nounits
if ($LASTEXITCODE -ne 0) { throw 'nvidia-smi inventory failed' }
$gpuRows = @($gpuText | ConvertFrom-Csv -Header name,bdf,pci_device_dword,pci_subsystem_dword,vbios_version,driver_version,nvml_memory_mib,nvml_reserved_mib,display_active)
$adapters = @(Get-CimInstance Win32_VideoController | Select-Object Name,PNPDeviceID,DriverVersion)
$secureBoot = (Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State' -Name UEFISecureBootEnabled -ErrorAction SilentlyContinue).UEFISecureBootEnabled
$taskRomPath = Join-Path $taskRoot 'inputs/PNY-RTX4090.rom'
$taskRomExists = Test-Path -LiteralPath $taskRomPath -PathType Leaf
$report = [ordered]@{
    captured_at = (Get-Date).ToString('o')
    method = 'Read-only nvidia-smi query, CIM, and SecureBoot registry state'
    gpus = $gpuRows
    adapters = $adapters
    secure_boot_registry_enabled = $secureBoot
    vbios_rom_exported = $taskRomExists
    vbios_rom_path = if ($taskRomExists) { $taskRomPath } else { $null }
    vbios_rom_sha256 = if ($taskRomExists) { (Get-FileHash -LiteralPath $taskRomPath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    cold_mmio_measured = $false
    hardware_unlock_executed = $false
    boot_settings_modified = $false
    note = 'NVML/WDDM memory totals are not the cold 0x1183a4 register. Chip ID, BAR, PROM and EFI RomImage must be measured before enabling the candidate.'
}
$outputPath = Join-Path $taskRoot 'inventory.json'
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outputPath -Encoding utf8
Write-Output "Saved read-only inventory to $outputPath"
