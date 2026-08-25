#!/usr/bin/env pwsh
<#
Batch-flash blank CANBoard v2 (STM32F303K8T6) over SWD via pyocd.

Loop: wait for board -> confirm flash is blank -> flash -> beep -> wait for unplug -> repeat.
Refuses any board that is not blank unless -Force.

  .\flash-canboard.ps1 -Hex C:/Users/you/Downloads/canboard_v2_FW_v0-5-8.hex

-Hex takes one or more images. A standalone firmware links its .vectors at 0x08000000 and
boots on its own -- flash it ALONE. A bootloader+app pair is two images whose regions must
not overlap, e.g. OpenBLT at 0x08000000 plus an app relocated to 0x08004000:
  -Hex bootloader/canboard/bin/canboard_blt.hex,build/canboard_v2.hex
Check where an image actually lands with:  arm-none-eabi-objdump -h <file.elf>

Flash map (64K part):
  0x08000000 + 62K   code
  0x0800F800 +  2K   persistent config -- '-e sector' leaves it alone

"No ACK" on every attempt usually means the board is UNPOWERED -- the Debugprobe does not
supply target power. Check that before chasing -Frequency.

Ctrl-C to stop.
#>
[CmdletBinding(DefaultParameterSetName = 'Flash')]
param(
    # Image(s) to flash. .hex/.elf carry their own load addresses; a raw .bin needs
    # '@0x08000000' appended to the filename.
    [Parameter(Mandatory, Position = 0, ParameterSetName = 'Flash')]
    [string[]]$Hex,
    [string]$Target  = 'stm32f303k8',
    [int]$FlashSize  = 0x10000,
    # SWD clock. Lower this (500k, 250k) if you get intermittent "No ACK" on long pigtails.
    [string]$Frequency = '1M',
    # Attempts at the flash step before calling the board bad. The link is often marginal.
    [int]$Retries = 2,
    # Flash even when the chip is not blank (reflashing an already-programmed board).
    [switch]$Force,
    # Do one board and exit, instead of looping. For bench-testing the script.
    [switch]$Once,
    [Parameter(Mandatory, ParameterSetName = 'SelfTest')]
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

# Returns the offset of the first non-erased byte, or -1 if the whole buffer is 0xFF.
function Get-FirstProgrammedOffset([byte[]]$Bytes) {
    for ($i = 0; $i -lt $Bytes.Length; $i++) {
        if ($Bytes[$i] -ne 0xFF) { return $i }
    }
    return -1
}

if ($SelfTest) {
    $blank = [byte[]]::new(64); for ($i = 0; $i -lt 64; $i++) { $blank[$i] = 0xFF }
    if ((Get-FirstProgrammedOffset $blank) -ne -1) { throw 'SelfTest: all-0xFF buffer reported as programmed' }
    $dirty = $blank.Clone(); $dirty[17] = 0x00
    if ((Get-FirstProgrammedOffset $dirty) -ne 17) { throw 'SelfTest: wrong offset for programmed byte' }
    # The bug this guards: 'pyocd cmd' exits 0 even on SWD "No ACK", so presence is judged on
    # output, never $LASTEXITCODE. These are real captured pyocd lines.
    $present = 'Core 0 (Cortex-M4):  Running'
    $absent  = "0000776 E Error while initing target: Error while running debug sequence 'DebugPortSetup' (core Cortex-M4): SWD/JTAG communication failure (No ACK) [commander]"
    if ($present -notmatch 'Core\s+\d+\s*\(') { throw 'SelfTest: live target not detected as present' }
    if ($absent  -match   'Core\s+\d+\s*\(') { throw 'SelfTest: No-ACK output misread as a present target' }
    # Guards the savemem path-mangling bug: backslashes must not reach the commander.
    if (('C:\Temp\x.bin' -replace '\\', '/') -ne 'C:/Temp/x.bin') { throw 'SelfTest: path not slash-normalised for savemem' }
    'SelfTest OK'; exit 0
}

# Read all of flash to $Path. Doubles as the presence test: a full-size dump proves the board
# is there AND readable in one connection. Judged on the artifact because 'pyocd cmd' exits 0
# even when SWD gets no ACK.
function Read-Flash([string]$Mode, [string]$Path) {
    Remove-Item $Path -ErrorAction SilentlyContinue
    # pyocd's commander eats '\' as an escape, so a Windows path silently becomes
    # 'C:UserstlmAppData...' -- it then reports "Saved 65536 bytes" for a file that
    # landed somewhere else. Forward slashes survive the tokenizer.
    $swd = $Path -replace '\\', '/'
    pyocd cmd -t $Target -M $Mode -f $Frequency -c "savemem 0x08000000 $FlashSize $swd" *> $null
    if (-not (Test-Path $Path)) { return $false }
    return (Get-Item $Path).Length -eq $FlashSize
}

# Blank parts hardfault-loop on a 0xFFFFFFFF reset vector, so under-reset is tried first;
# 'halt' is the fallback for probes whose nRST is not wired through.
$connectModes = @('under-reset', 'halt')

# Lightweight presence poll for the unplug wait -- no flash read, and 'attach' leaves a
# freshly flashed board running.
function Test-TargetPresent {
    $out = pyocd cmd -t $Target -M attach -f $Frequency -c 'status' 2>&1 | Out-String
    return $out -match 'Core\s+\d+\s*\('
}

foreach ($f in $Hex) {
    if (-not (Test-Path ($f -replace '@.*$', ''))) { throw "Missing image: $f" }
}
if (-not (pyocd list 2>&1 | Select-String -Quiet 'Unique ID')) {
    throw 'No debug probe found by pyocd. Plug in the Debugprobe first.'
}
Write-Host "image  : $($Hex -join ', ')"
Write-Host "target : $Target @ $Frequency`n"

$dump = Join-Path ([IO.Path]::GetTempPath()) 'canboard-blankcheck.bin'
$count = 0

while ($true) {
    # One connection per attempt: the flash readback IS the presence check, so the link can't
    # pass a probe step and then fail the real one.
    Write-Host 'Waiting for board...' -NoNewline
    $mode = $null
    while (-not $mode) {
        foreach ($m in $connectModes) {
            if (Read-Flash $m $dump) { $mode = $m; break }
        }
        if (-not $mode) { Start-Sleep -Milliseconds 500 }
    }
    Write-Host "`rRead 64K via $mode.          "

    # try/catch so one bad board never takes the batch loop down with it.
    try {
        $offset = Get-FirstProgrammedOffset ([IO.File]::ReadAllBytes($dump))
        if ($offset -ge 0 -and -not $Force) {
            Write-Host ("SKIP  not blank: first programmed byte at 0x{0:X8}. Use -Force to reflash." -f (0x08000000 + $offset)) -ForegroundColor Yellow
            [Console]::Beep(330, 400)
        }
        else {
            if ($offset -ge 0) { Write-Host 'Not blank, -Force given: reflashing.' -ForegroundColor Yellow }
            $flashed = $false
            foreach ($attempt in 1..$Retries) {
                # 'pyocd load' DOES exit nonzero on failure, and verifies by readback
                # unless --trust-crc -- so here the exit code is trustworthy.
                pyocd load -t $Target -M $mode -f $Frequency -e sector @Hex
                if ($LASTEXITCODE -eq 0) { $flashed = $true; break }
                Write-Host "  attempt $attempt/$Retries failed, retrying..." -ForegroundColor Yellow
            }
            if ($flashed) {
                $count++
                Write-Host ("PASS  board #{0} flashed + verified at {1:HH:mm:ss}" -f $count, (Get-Date)) -ForegroundColor Green
                [Console]::Beep(880, 150)
            }
            else {
                Write-Host "FAIL  $Retries attempts failed -- board NOT programmed" -ForegroundColor Red
                [Console]::Beep(220, 400)
            }
        }
    }
    catch {
        Write-Host "FAIL  $($_.Exception.Message)" -ForegroundColor Red
        [Console]::Beep(220, 400)
    }

    if ($Once) { break }

    Write-Host 'Unplug board...' -NoNewline
    while (Test-TargetPresent) { Start-Sleep -Milliseconds 500 }
    Write-Host "`rBoard removed. $count passed so far.`n"
}
