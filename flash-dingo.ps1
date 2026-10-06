#!/usr/bin/env pwsh
<#
Batch-flash blank dingo boards over SWD via pyocd. Supports CANBoard v2 (STM32F303K8) and
dingoPDM v7 / dingoPDM-Max v1 (STM32F446).

Loop: wait for board -> identify the MCU -> check the image belongs on it -> confirm the flash
is blank -> flash -> beep -> wait for unplug -> repeat.

  .\flash-dingo.ps1 -Hex C:/Users/you/Downloads/canboard_v2_FW_v0-5-8.hex

The board is IDENTIFIED from its DBGMCU_IDCODE, not assumed, and the image is checked against
it before anything is written: it must fit the part's real flash size, its filename must not
name a different board, and multiple images must not overlap each other. A mismatch is refused,
so PDM firmware cannot land on a CANBoard or vice versa.

-Hex takes one or more images. A standalone firmware links its .vectors at 0x08000000 and boots
on its own -- flash it ALONE. A bootloader+app pair is two non-overlapping images, e.g. OpenBLT
at 0x08000000 plus an app relocated to 0x08004000 (CANBoard) / 0x08004000 +368K (PDM):
  -Hex bootloader/canboard/bin/canboard_blt.hex,build/canboard_v2.hex
Check where an image lands with:  arm-none-eabi-objdump -h <file.elf>

ERASE SCOPE -- '-e sector' erases every SECTOR an image touches, in full. Not just the bytes
written, and not only on -Force: -Force merely skips the blank check, it changes nothing about
erasing. What gets erased follows the image's address span and the part's sector map:

  * A STANDALONE image starts at 0x08000000, so it erases and REPLACES any OpenBLT bootloader
    there. That board loses CAN update -- SWD/DFU only afterwards. Flash the relocated app
    instead (0x08004000) and sector 0 is untouched, keeping "SWD once, then CAN forever".
  * F446 sectors run 16/16/16/16/64/128/128/128 KB, so the 159 KB PDM image reaches into
    sector 5 and erases 256 KB -- ~98 KB beyond the image. Anything in 0x08028000-0x0803FFFF
    is destroyed even though nothing is written there.
  * Config survives in both boards: PDM sector 7 (0x08060000) is past the erase, and the
    CANBoard's 2 KB sector 31 (0x0800F800) sits above its 62 KB image.

VERIFY -- pyocd does NOT read the image back after programming ('--trust-crc' only decides which
pages to skip BEFORE writing). This script therefore re-reads the image span itself and compares
it to the .hex byte for byte, and only reports PASS when that matches. A non-.hex image cannot be
compared, and says so rather than claiming a verify it did not do.

"No ACK" on every mode and every clock usually means the board is UNPOWERED -- the Debugprobe
does not supply target power. Check that, and the shared ground, before chasing -Frequency.

Ctrl-C to stop.
#>
[CmdletBinding(DefaultParameterSetName = 'Flash')]
param(
    # Image(s) to flash. .hex/.elf carry their own load addresses; a raw .bin needs
    # '@0x08000000' appended to the filename.
    [Parameter(Mandatory, Position = 0, ParameterSetName = 'Flash')]
    [string[]]$Hex,
    # pyocd target name. Default is whatever the connected MCU turns out to be.
    [string]$Target,
    # SWD clock. Read and program time scale with this, so it defaults high; the script falls
    # back down $FALLBACK_CLOCKS automatically if the target won't answer at this speed.
    [string]$Frequency = '4M',
    # Verify by CRC32 instead of reading the whole image back. Noticeably quicker on a
    # hand-held probe; a CRC32 match is a weaker guarantee than a byte-for-byte compare.
    [switch]$Fast,
    # Attempts at the flash step before calling the board bad. The link is often marginal.
    [int]$Retries = 2,
    # Flash even when the chip is not blank (reflashing an already-programmed board).
    [switch]$Force,
    # Flash even when the image looks wrong for the detected board. Last resort.
    [switch]$IgnoreMismatch,
    # Skip the one-time "flash this image onto these boards?" prompt at startup.
    [switch]$Yes,
    # Do one board and exit, instead of looping. For bench-testing the script.
    [switch]$Once,
    [Parameter(Mandatory, ParameterSetName = 'SelfTest')]
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

# Tried in order when the target won't answer, so a too-fast default degrades instead of
# looking like a dead board. First entry is whatever -Frequency asked for.
$FALLBACK_CLOCKS = @($Frequency, '2M', '1M', '500k') | Select-Object -Unique
$script:clock = $Frequency

# DBGMCU_IDCODE lives at 0xE0042000 on both F3 and F4. DEV_ID is the low 12 bits.
# FlashSizeReg holds flash size in KB and differs per family.
$PARTS = @{
    0x438 = @{ Name = 'STM32F303x6/x8'; Board = 'CANBoard v2';                 Target = 'stm32f303k8'; FlashSizeReg = 0x1FFFF7CC; Keyword = 'canboard' }
    0x422 = @{ Name = 'STM32F303xB/C';  Board = 'CANBoard (larger F303)';      Target = 'stm32f303cc'; FlashSizeReg = 0x1FFFF7CC; Keyword = 'canboard' }
    0x421 = @{ Name = 'STM32F446';      Board = 'dingoPDM v7 / -Max v1';       Target = 'stm32f446re'; FlashSizeReg = 0x1FFF7A22; Keyword = 'dingopdm' }
    0x413 = @{ Name = 'STM32F405/407';  Board = 'dingoPDM (F405 variant)';     Target = 'stm32f405rg'; FlashSizeReg = 0x1FFF7A22; Keyword = 'dingopdm' }
}

# Returns the offset of the first non-erased byte, or -1 if the whole buffer is 0xFF.
function Get-FirstProgrammedOffset([byte[]]$Bytes) {
    for ($i = 0; $i -lt $Bytes.Length; $i++) {
        if ($Bytes[$i] -ne 0xFF) { return $i }
    }
    return -1
}

# Parse an Intel HEX file into its address extent AND its bytes, so the image can be checked
# against the part before writing and compared against the chip afterwards. $null for formats we
# can't read. Data/Mask are indexed from Start; Mask marks the addresses the file actually
# defines, so gaps between records are never compared.
function Get-HexExtent([string]$Path) {
    if ($Path -notmatch '\.hex$') { return $null }
    $lo = [uint32]::MaxValue; $hi = [uint32]0; $bytes = 0; $upper = [uint32]0
    $records = [Collections.Generic.List[object]]::new()
    foreach ($line in [IO.File]::ReadLines($Path)) {
        if ($line -notmatch '^:[0-9A-Fa-f]{10}') { continue }
        $len  = [Convert]::ToInt32($line.Substring(1, 2), 16)
        $addr = [Convert]::ToInt32($line.Substring(3, 4), 16)
        $type = [Convert]::ToInt32($line.Substring(7, 2), 16)
        switch ($type) {
            0 {   # data record
                $abs = $upper + $addr
                if ($abs -lt $lo) { $lo = $abs }
                if (($abs + $len) -gt $hi) { $hi = $abs + $len }
                $bytes += $len
                $payload = [byte[]]::new($len)
                for ($k = 0; $k -lt $len; $k++) {
                    $payload[$k] = [Convert]::ToByte($line.Substring(9 + ($k * 2), 2), 16)
                }
                $records.Add(@{ Addr = $abs; Payload = $payload })
            }
            4 { $upper = [uint32]([Convert]::ToInt32($line.Substring(9, 4), 16)) -shl 16 }  # extended linear address
        }
    }
    if ($hi -eq 0) { return $null }
    $data = [byte[]]::new($hi - $lo)
    $mask = [bool[]]::new($hi - $lo)
    foreach ($r in $records) {
        $at = $r.Addr - $lo
        for ($k = 0; $k -lt $r.Payload.Length; $k++) {
            $data[$at + $k] = $r.Payload[$k]
            $mask[$at + $k] = $true
        }
    }
    return @{ Start = $lo; End = $hi; Bytes = $bytes; Data = $data; Mask = $mask }
}

# Compare a flash readback against what the image files say should be there. Only addresses the
# files actually define are checked, so record gaps are ignored. pyocd does NOT verify after
# programming -- '--trust-crc' only decides which pages to skip BEFORE writing -- so without this
# a "flashed" board is unconfirmed.
# Returns @{ Ok; Checked; BadCount; FirstBad } ($null address when everything matched).
function Test-FlashAgainstImages($Images, [long]$From, [byte[]]$Actual) {
    $checked = 0; $bad = 0; $firstBad = $null
    foreach ($img in $Images) {
        for ($i = 0; $i -lt $img.Mask.Length; $i++) {
            if (-not $img.Mask[$i]) { continue }
            $idx = ($img.Start + $i) - $From
            if ($idx -lt 0 -or $idx -ge $Actual.Length) { continue }
            $checked++
            if ($Actual[$idx] -ne $img.Data[$i]) {
                $bad++
                if ($null -eq $firstBad) { $firstBad = $img.Start + $i }
            }
        }
    }
    return @{ Ok = ($bad -eq 0 -and $checked -gt 0); Checked = $checked; BadCount = $bad; FirstBad = $firstBad }
}

# Does this image belong on this part? Returns a list of reasons it does not.
function Get-ImageComplaints($Part, [string[]]$Images) {
    $problems = @()
    $flashEnd = 0x08000000 + $Part.FlashSize
    $extents = @()
    foreach ($img in $Images) {
        $file = $img -replace '@.*$', ''
        $name = [IO.Path]::GetFileName($file)

        # Filename naming a different board is the clearest possible mismatch signal.
        # Unique keywords only -- several DEV_IDs share one board family (two F303 variants
        # both mean 'canboard'), which would otherwise report the same complaint twice.
        $otherKeywords = $PARTS.Values.Keyword | Sort-Object -Unique | Where-Object { $_ -ne $Part.Keyword }
        foreach ($kw in $otherKeywords) {
            if ($name -match $kw) {
                $problems += "'$name' names '$kw' but the connected part is $($Part.Name) ($($Part.Board))"
            }
        }

        $e = Get-HexExtent $file
        if ($null -eq $e) { continue }   # .bin/.elf: extent unknown, size check skipped
        $extents += @{ Name = $name; Extent = $e }
        if ($Part.FlashSize -gt 0 -and $e.End -gt $flashEnd) {
            $problems += ("'{0}' reaches 0x{1:X8}, past this part's flash end 0x{2:X8} ({3} KB)" -f `
                $name, $e.End, $flashEnd, ($Part.FlashSize / 1024))
        }
        if ($e.Start -lt 0x08000000) {
            $problems += ("'{0}' starts at 0x{1:X8}, below flash base 0x08000000" -f $name, $e.Start)
        }
    }
    # Two images writing the same bytes means one silently wins.
    for ($i = 0; $i -lt $extents.Count; $i++) {
        for ($j = $i + 1; $j -lt $extents.Count; $j++) {
            $a = $extents[$i]; $b = $extents[$j]
            if ($a.Extent.Start -lt $b.Extent.End -and $b.Extent.Start -lt $a.Extent.End) {
                $problems += "'$($a.Name)' and '$($b.Name)' overlap in flash -- flash them separately"
            }
        }
    }
    return $problems
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
    # IDCODE decode: DEV_ID is the low 12 bits. 0x10016438 is a real read off a CANBoard.
    if ((0x10016438 -band 0xFFF) -ne 0x438) { throw 'SelfTest: DEV_ID mask wrong' }
    if ($PARTS[0x438].Target -ne 'stm32f303k8') { throw 'SelfTest: F303 part table wrong' }
    if ($PARTS[0x421].Target -ne 'stm32f446re') { throw 'SelfTest: F446 part table wrong' }
    # Guards the hex-literal sign trap: 0xE0042000 is a negative Int32 in PowerShell.
    if (0xE0042000L -lt 0) { throw 'SelfTest: IDCODE address literal lost its sign' }
    # Guards the scalar-.Count trap that disabled the fast blank check: one hashtable piped out
    # of ForEach-Object is not a 1-element array, and its .Count is the number of keys.
    if (@(@{a=1;b=2;c=3}).Count -ne 1) { throw 'SelfTest: single extent not coerced to a 1-element array' }
    # Intel HEX parser, against a 3-record file: 16 bytes at 0x08000000 via a type-04 record.
    $tmp = Join-Path ([IO.Path]::GetTempPath()) 'selftest.hex'
    @(':020000040800F2', ':1000000000040020890200088B0200088B0200080F', ':00000001FF') | Set-Content $tmp
    $e = Get-HexExtent $tmp
    if ($e.Start -ne 0x08000000) { throw "SelfTest: hex start $($e.Start) != 0x08000000" }
    if ($e.End -ne 0x08000010)   { throw "SelfTest: hex end $($e.End) != 0x08000010" }
    if ($e.Bytes -ne 16)         { throw "SelfTest: hex payload $($e.Bytes) != 16" }

    # --- the guard that protects boards: does an image belong on this part? ---
    $f303 = $PARTS[0x438].Clone(); $f303.FlashSize = 64 * 1024
    $f446 = $PARTS[0x421].Clone(); $f446.FlashSize = 512 * 1024
    $dir  = Join-Path ([IO.Path]::GetTempPath()) 'dingo-selftest'
    $null = New-Item -ItemType Directory -Path $dir -Force
    # 16 bytes at 0x08000000, and 16 bytes at 0x08004000 -- adjacent, non-overlapping.
    $at0  = Join-Path $dir 'canboard_blt.hex'
    $at16 = Join-Path $dir 'canboard_app.hex'
    $big  = Join-Path $dir 'dingopdm_fw.hex'
    @(':020000040800F2', ':1000000000040020890200088B0200088B0200080F', ':00000001FF') | Set-Content $at0
    @(':020000040800F2', ':1000400000040020890200088B0200088B0200080F', ':00000001FF') | Set-Content $at16
    # 16 bytes at 0x08010000 -- one sector past the end of a 64K F303, fine on a 512K F446
    @(':020000040801F1', ':1000000000040020890200088B0200088B0200080F', ':00000001FF') | Set-Content $big

    if ((Get-ImageComplaints $f303 @($at0)).Count -ne 0) { throw 'SelfTest: rejected a valid image' }
    if ((Get-ImageComplaints $f303 @($at0, $at16)).Count -ne 0) { throw 'SelfTest: rejected a valid non-overlapping pair' }
    if (-not (Get-ImageComplaints $f303 @($at0, $at0) | Where-Object { $_ -match 'overlap' })) { throw 'SelfTest: overlap not detected' }
    if (-not (Get-ImageComplaints $f303 @($big) | Where-Object { $_ -match 'past this part' })) { throw 'SelfTest: oversized image not caught on 64K part' }
    if (-not (Get-ImageComplaints $f303 @($big) | Where-Object { $_ -match "names 'dingopdm'" })) { throw 'SelfTest: PDM image not caught by name on a CANBoard' }
    if (Get-ImageComplaints $f446 @($big) | Where-Object { $_ -match 'past this part' }) { throw 'SelfTest: same image wrongly rejected on the 512K part' }
    if (-not (Get-ImageComplaints $f446 @($at0) | Where-Object { $_ -match "names 'canboard'" })) { throw 'SelfTest: CANBoard image not caught by name on a PDM' }

    # --- the post-flash verify: pyocd does not read back, so this is the only real check ---
    $img = Get-HexExtent $at0                       # 16 bytes at 0x08000000
    if ($img.Data.Length -ne 16 -or -not $img.Mask[0]) { throw 'SelfTest: hex bytes not captured' }
    if ($img.Data[0] -ne 0x00 -or $img.Data[3] -ne 0x20) { throw 'SelfTest: hex payload decoded wrong' }
    $good = [byte[]]::new(32); [Array]::Copy($img.Data, $good, 16)
    $v = Test-FlashAgainstImages @($img) $img.Start $good
    if (-not $v.Ok -or $v.Checked -ne 16) { throw "SelfTest: verify rejected a correct readback ($($v.BadCount) bad)" }
    $wrong = $good.Clone(); $wrong[5] = $good[5] -bxor 0xFF
    $v = Test-FlashAgainstImages @($img) $img.Start $wrong
    if ($v.Ok) { throw 'SelfTest: verify accepted a corrupted readback' }
    if ($v.BadCount -ne 1 -or $v.FirstBad -ne ($img.Start + 5)) { throw "SelfTest: verify misreported the bad byte ($($v.BadCount) at $($v.FirstBad))" }
    # A gap between records must never be compared: mask it out and corrupt it.
    $img.Mask[7] = $false
    $wrong2 = $good.Clone(); $wrong2[7] = 0xAA
    if (-not (Test-FlashAgainstImages @($img) $img.Start $wrong2).Ok) { throw 'SelfTest: verify compared an undefined gap byte' }

    Remove-Item $tmp, $dir -Recurse -ErrorAction SilentlyContinue
    'SelfTest OK'; exit 0
}

# Read a 32-bit word using the generic cortex_m target, so this works before we know the part.
# 'pyocd cmd' exits 0 even on SWD failure, so success is judged on the output format.
# $Address is [long] on purpose: PowerShell parses a hex literal like 0xE0042000 as a NEGATIVE
# Int32 (the high bit wraps), so [uint32] would reject it outright.
function Read-Word([long]$Address) {
    $out = pyocd cmd -t cortex_m -M attach -f $script:clock -c ("read32 0x{0:X8}" -f $Address) 2>&1 | Out-String
    if ($out -match '(?m)^[0-9a-f]{8}:\s+([0-9a-f]{8})') { return [Convert]::ToUInt32($Matches[1], 16) }
    return $null
}

# Find the fastest clock in $FALLBACK_CLOCKS the target actually answers on, and stick to it.
# Without this a high -Frequency default would be indistinguishable from an absent board.
function Set-WorkingClock {
    foreach ($f in $FALLBACK_CLOCKS) {
        $script:clock = $f
        if ($null -ne (Read-Word 0xE0042000L)) {
            if ($f -ne $FALLBACK_CLOCKS[0]) { Write-Host "  (fell back to $f SWD)" -ForegroundColor DarkGray }
            return $true
        }
    }
    $script:clock = $FALLBACK_CLOCKS[0]
    return $false
}

# Identify the connected MCU from its IDCODE, then read its true flash size.
function Get-ConnectedPart {
    $idcode = Read-Word 0xE0042000L
    if ($null -eq $idcode) { return $null }
    $devId = $idcode -band 0xFFF
    $p = $PARTS[[int]$devId]
    if (-not $p) {
        return @{ Unknown = $true; DevId = $devId; Rev = ($idcode -shr 16) }
    }
    $kb = Read-Word $p.FlashSizeReg
    $part = $p.Clone()
    $part.DevId     = $devId
    $part.Rev       = ($idcode -shr 16)
    $part.FlashSize = if ($null -ne $kb) { ($kb -band 0xFFFF) * 1024 } else { 0 }
    return $part
}

# Read all of flash to $Path. Doubles as a liveness test: a full-size dump proves the board is
# there AND readable in one connection. Judged on the artifact because 'pyocd cmd' exits 0 even
# when SWD gets no ACK, and because the commander eats '\' as an escape -- a Windows path
# silently becomes 'C:UserstlmAppData...' and it reports "Saved N bytes" for a file written
# somewhere else. Forward slashes survive the tokenizer.
function Read-Flash([string]$TargetName, [string]$Mode, [long]$Start, [int]$Size, [string]$Path) {
    Remove-Item $Path -ErrorAction SilentlyContinue
    $swd = $Path -replace '\\', '/'
    pyocd cmd -t $TargetName -M $Mode -f $script:clock -c ("savemem 0x{0:X8} {1} {2}" -f $Start, $Size, $swd) *> $null
    if (-not (Test-Path $Path)) { return $false }
    return (Get-Item $Path).Length -eq $Size
}


foreach ($f in $Hex) {
    if (-not (Test-Path ($f -replace '@.*$', ''))) { throw "Missing image: $f" }
}
if (-not (pyocd list 2>&1 | Select-String -Quiet 'Unique ID')) {
    throw 'No debug probe found by pyocd. Plug in the Debugprobe first.'
}

Write-Host 'image(s):'
foreach ($f in $Hex) {
    $e = Get-HexExtent ($f -replace '@.*$', '')
    if ($e) {
        Write-Host ("  {0}  0x{1:X8}-0x{2:X8}  {3:N0} bytes" -f [IO.Path]::GetFileName($f), $e.Start, $e.End, $e.Bytes)
    }
    else {
        Write-Host ("  {0}  (extent unknown -- not a .hex, size check skipped)" -f [IO.Path]::GetFileName($f))
    }
}

# Blank-check only the span the images actually occupy. Reading the whole chip was the single
# biggest cost -- 512K on an F446 is ~13s of probe-holding, versus ~4s for a 162K image -- and
# bytes outside this span are never written, so their state cannot matter.
# $null extent (.bin/.elf) means we don't know the span, so fall back to the whole flash.
# @() is load-bearing: a single extent would otherwise be a bare hashtable, whose .Count is its
# key count (3), not 1 -- which silently sent every single-image run down the whole-flash path.
$extents = @($Hex | ForEach-Object { Get-HexExtent ($_ -replace '@.*$', '') } | Where-Object { $_ })
$expected = if ($extents.Count -eq @($Hex).Count) { $extents } else { $null }
if ($extents.Count -eq @($Hex).Count) {
    # [long] casts matter: Measure-Object hands back a Double, which the X8 format specifier
    # cannot render at all.
    $checkStart = [long]($extents | Measure-Object -Property Start -Minimum).Minimum
    $checkEnd   = [long]($extents | Measure-Object -Property End   -Maximum).Maximum
    Write-Host ("blank check: 0x{0:X8}-0x{1:X8} ({2:N0} KB)" -f $checkStart, $checkEnd, (($checkEnd - $checkStart) / 1024))
}
else {
    $checkStart = $null   # resolved per board, once its flash size is known
    Write-Host 'blank check: whole flash (some images are not .hex, so their span is unknown)'
}
Write-Host ''

# Blank parts hardfault-loop on a 0xFFFFFFFF reset vector, so under-reset is tried first;
# 'halt' is the fallback for probes whose nRST is not wired through.
$connectModes = @('under-reset', 'halt')
$dump = Join-Path ([IO.Path]::GetTempPath()) 'dingo-blankcheck.bin'
$count = 0
$confirmed = $Yes

while ($true) {
    Write-Host 'Waiting for board...' -NoNewline
    $part = $null
    while (-not $part) {
        if (Set-WorkingClock) { $part = Get-ConnectedPart }
        if (-not $part) { Start-Sleep -Milliseconds 500 }
    }

    if ($part.Unknown) {
        Write-Host ("`rUNKNOWN part: DEV_ID 0x{0:X3}, rev 0x{1:X4} -- not a board this script knows." -f $part.DevId, $part.Rev) -ForegroundColor Red
        [Console]::Beep(220, 400)
        if ($Once) { break }
        Write-Host 'Unplug board...' -NoNewline
        while (Get-ConnectedPart) { Start-Sleep -Milliseconds 500 }
        Write-Host "`rBoard removed.`n"
        continue
    }

    $tgt = if ($Target) { $Target } else { $part.Target }
    Write-Host ("`rConnected: {0}  ({1})  rev 0x{2:X4}  {3} KB flash  -> pyocd '{4}'" -f `
        $part.Name, $part.Board, $part.Rev, ($part.FlashSize / 1024), $tgt) -ForegroundColor Cyan

    # --- does the image belong on THIS board? ---
    $complaints = Get-ImageComplaints $part $Hex
    if ($complaints) {
        Write-Host 'MISMATCH between image and connected board:' -ForegroundColor Red
        $complaints | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
        if (-not $IgnoreMismatch) {
            Write-Host 'SKIP  refusing to flash. Use -IgnoreMismatch to override.' -ForegroundColor Red
            [Console]::Beep(220, 400)
            if ($Once) { break }
            Write-Host 'Unplug board...' -NoNewline
            while (Get-ConnectedPart) { Start-Sleep -Milliseconds 500 }
            Write-Host "`rBoard removed.`n"
            continue
        }
        Write-Host '  -IgnoreMismatch given: proceeding anyway.' -ForegroundColor Yellow
    }

    # One confirmation for the whole batch, once the first board has identified itself.
    if (-not $confirmed) {
        $answer = Read-Host "Flash the image(s) above onto $($part.Board) boards? [y/N]"
        if ($answer -notmatch '^(y|yes)$') { Write-Host 'Aborted.'; exit 1 }
        $confirmed = $true
    }

    $verify = $null
    try {
        # The flash readback is also the liveness check, so a marginal link cannot pass a probe
        # step and then fail the real one.
        $from = if ($null -ne $checkStart) { $checkStart } else { 0x08000000 }
        $size = if ($null -ne $checkStart) { [int]($checkEnd - $checkStart) } else { $part.FlashSize }
        $mode = $null
        foreach ($m in $connectModes) {
            if (Read-Flash $tgt $m $from $size $dump) { $mode = $m; break }
        }
        if (-not $mode) {
            Write-Host 'FAIL  could not read flash back -- reseat the board or lower -Frequency' -ForegroundColor Red
            [Console]::Beep(220, 400)
        }
        else {
            $offset = Get-FirstProgrammedOffset ([IO.File]::ReadAllBytes($dump))
            if ($offset -ge 0 -and -not $Force) {
                Write-Host ("SKIP  not blank: first programmed byte at 0x{0:X8}. Use -Force to reflash." -f ($from + $offset)) -ForegroundColor Yellow
                [Console]::Beep(330, 400)
            }
            else {
                if ($offset -ge 0) { Write-Host 'Not blank, -Force given: reflashing.' -ForegroundColor Yellow }
                $flashed = $false
                foreach ($attempt in 1..$Retries) {
                    # 'pyocd load' exits nonzero if programming itself fails, but it does NOT
                    # read the image back afterwards -- '--trust-crc' only decides which pages to
                    # skip BEFORE writing. The verify below is what actually confirms the board.
                    # '-e sector' erases every SECTOR the image touches, in full -- not just the
                    # bytes written. See the erase notes in the header.
                    if ($Fast) { pyocd load -t $tgt -M $mode -f $script:clock -e sector --trust-crc @Hex }
                    else       { pyocd load -t $tgt -M $mode -f $script:clock -e sector @Hex }
                    if ($LASTEXITCODE -ne 0) {
                        Write-Host "  attempt $attempt/$Retries failed to program, retrying..." -ForegroundColor Yellow
                        continue
                    }
                    # --- read the image span back and compare it to the files, byte for byte ---
                    if ($null -eq $expected) {
                        Write-Host '  programmed, but image bytes are unknown (not a .hex) -- cannot verify' -ForegroundColor Yellow
                        $flashed = $true; break
                    }
                    if (-not (Read-Flash $tgt $mode $from $size $dump)) {
                        Write-Host "  attempt $attempt/$Retries programmed but could not read back, retrying..." -ForegroundColor Yellow
                        continue
                    }
                    $v = Test-FlashAgainstImages $expected $from ([IO.File]::ReadAllBytes($dump))
                    if ($v.Ok) { $flashed = $true; $verify = $v; break }
                    Write-Host ("  attempt {0}/{1} VERIFY FAILED: {2:N0} of {3:N0} bytes wrong, first at 0x{4:X8}" -f `
                        $attempt, $Retries, $v.BadCount, $v.Checked, $v.FirstBad) -ForegroundColor Yellow
                }
                if ($flashed) {
                    $count++
                    $what = if ($verify) { "{0:N0} bytes verified" -f $verify.Checked } else { 'NOT verified' }
                    Write-Host ("PASS  {0} #{1} flashed, {2}, at {3:HH:mm:ss}" -f $part.Board, $count, $what, (Get-Date)) -ForegroundColor Green
                    [Console]::Beep(880, 150)
                }
                else {
                    Write-Host "FAIL  $Retries attempts failed -- board is NOT correctly programmed" -ForegroundColor Red
                    [Console]::Beep(220, 400)
                }
            }
        }
    }
    catch {
        Write-Host "FAIL  $($_.Exception.Message)" -ForegroundColor Red
        [Console]::Beep(220, 400)
    }

    if ($Once) { break }

    Write-Host 'Unplug board...' -NoNewline
    while (Get-ConnectedPart) { Start-Sleep -Milliseconds 500 }
    Write-Host "`rBoard removed. $count passed so far.`n"
}
