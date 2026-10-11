#!/usr/bin/env pwsh
<#
Batch-flash blank dingo boards over SWD (pyocd) or USB DFU (dfu-util). Supports CANBoard v2
(STM32F303K8, SWD only: it has no USB DFU) and dingoPDM v7 / dingoPDM-Max v1 (STM32F446, SWD or
its USB-C port in the ST ROM bootloader: hold BOOT0 while plugging in).

Loop: wait for a board on either link -> identify the MCU -> check the image belongs on it ->
confirm the flash is blank -> flash -> verify -> beep -> wait for unplug -> repeat. Both links are
watched at once, so CANBoards on the probe and PDMs on USB can be mixed in one session; pass the
images for both and each board gets the ones that belong on it.

  .\flash-dingo.ps1                                                # asks which release to download
  .\flash-dingo.ps1 -Source Official                               # corygrant/dingoFW latest
  .\flash-dingo.ps1 -Hex build/canboard_v2.hex                     # CANBoards on the probe
  .\flash-dingo.ps1 -Hex build/dingopdm_v7.hex                     # PDMs on USB-C (or the probe)
  .\flash-dingo.ps1 -Hex build/canboard_v2.hex,build/dingopdm_v7.hex   # a mixed batch
  .\flash-dingo.ps1 -Hex build/dingopdm_v7.hex -Via Usb            # ignore the probe

USB DFU identifies the part from the ROM bootloader's flash layout string (an F446 reports
4x16K+64K+3x128K = 512K), takes .hex images (or '.bin@0x...'), erases the sectors each image
touches exactly like '-e sector' below, and is read back and compared like SWD. After PASS,
unplug and release BOOT0 so the board boots the new firmware.

dingoPDM v7 and -Max share the F446, so a PDM is told apart by the 'DINGO_BOARD=' marker in the
firmware already on it (CoffeeDingoFW builds after v5.5.108). A blank board, official firmware or
an older build has none, and the script asks v7 or Max (Enter repeats the last answer); -Pdm V7|Max
answers up front.

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
    # '@0x08000000' appended to the filename. Leave out to download a release instead (-Source).
    [Parameter(Position = 0, ParameterSetName = 'Flash')]
    [string[]]$Hex,
    # Without -Hex: which release to download. Asked at startup when not given (Stable on Enter).
    #   Stable   CoffeeDingoFW latest release, plus the OpenBLT bootloaders (CAN updates later)
    #   Testing  CoffeeDingoFW 'testing-latest' build, plus the bootloaders
    #   Official corygrant/dingoFW latest release (standalone, no CAN bootloader)
    [Parameter(ParameterSetName = 'Flash')]
    [ValidateSet('Stable', 'Testing', 'Official')]
    [string]$Source,
    # dingoPDM v7 or -Max. Both are an F446, so each PDM is told apart by the board marker in the
    # firmware already on it; when there is none (blank board, official or older firmware) the
    # script asks, unless this is given. Also limits a release download to that one PDM image.
    [Parameter(ParameterSetName = 'Flash')]
    [ValidateSet('V7', 'Max')]
    [string]$Pdm,
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
    # Which link(s) to watch: the SWD probe, USB DFU, or both (Auto, the default).
    [ValidateSet('Auto', 'Swd', 'Usb')]
    [string]$Via = 'Auto',
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

# The images meant for this part: those naming its board plus those naming no board, so a mixed
# batch (CANBoard + PDM images) gives each board only its own. When nothing fits, all of them go
# through, and Get-ImageComplaints says why they don't belong.
function Select-ImagesFor($Part, [string[]]$Images) {
    $others = $PARTS.Values.Keyword | Sort-Object -Unique | Where-Object { $_ -ne $Part.Keyword }
    $mine = @($Images | Where-Object {
        $n = [IO.Path]::GetFileName(($_ -replace '@.*$', ''))
        -not ($others | Where-Object { $n -match $_ })
    })
    if ($mine.Count) { return $mine }
    return $Images
}

# Where a set of images lands: the span to blank-check and read back, and the bytes to verify.
# Start is $null when an image isn't a .hex (span unknown, whole flash is used instead).
# Cached: parsing a 160 KB .hex takes PowerShell a few seconds, and a batch reuses the same set.
$script:planCache = @{}
function Get-ImagePlan([string[]]$Images) {
    $key = $Images -join '|'
    if ($script:planCache.ContainsKey($key)) { return $script:planCache[$key] }
    # @() is load-bearing: a single extent would otherwise be a bare hashtable, whose .Count is its
    # key count, not 1.
    $ext = @($Images | ForEach-Object { Get-HexExtent ($_ -replace '@.*$', '') } | Where-Object { $_ })
    $plan = if ($ext.Count -ne @($Images).Count) { @{ Expected = $null; Start = $null; End = $null } }
    else {
        # [long] casts matter: Measure-Object hands back a Double, which X8 cannot render.
        @{ Expected = $ext
           Start    = [long]($ext | Measure-Object -Property Start -Minimum).Minimum
           End      = [long]($ext | Measure-Object -Property End -Maximum).Maximum }
    }
    $script:planCache[$key] = $plan
    return $plan
}

# Total flash size from an ST ROM bootloader's DFU alt name, e.g.
# '@Internal Flash  /0x08000000/04*016Kg,01*064Kg,03*128Kg' -> 512 KB. 0 if it isn't one.
function Get-DfuFlashSize([string]$AltName) {
    if ($AltName -notmatch '/0x08000000/(\S+)') { return 0 }
    $total = 0
    foreach ($m in [regex]::Matches($Matches[1], '(\d+)\*(\d+)([KM])')) {
        $unit = if ($m.Groups[3].Value -eq 'M') { 1MB } else { 1KB }
        $total += [int]$m.Groups[1].Value * [int]$m.Groups[2].Value * $unit
    }
    return $total
}

# ST ROM bootloader flash size -> entry in $PARTS. Only USB-capable parts appear here; the
# CANBoard's F303K8 has no USB DFU.
$DFU_PARTS = @{ (512 * 1KB) = 0x421; (1024 * 1KB) = 0x413 }

# One image as a single block for dfu-util, which writes raw bytes at one address. Gaps between
# .hex records become 0xFF so they stay erased. $null for formats dfu-util can't place (.elf).
function Get-DfuImage([string]$Spec) {
    $file = $Spec -replace '@.*$', ''
    if ($Spec -match '@(0x[0-9A-Fa-f]+)$') {
        return @{ Start = [Convert]::ToInt64($Matches[1], 16); Data = [IO.File]::ReadAllBytes($file) }
    }
    $e = Get-HexExtent $file
    if (-not $e) { return $null }
    $data = $e.Data.Clone()
    for ($i = 0; $i -lt $data.Length; $i++) { if (-not $e.Mask[$i]) { $data[$i] = 0xFF } }
    return @{ Start = [long]$e.Start; Data = $data }
}

# Where a downloaded firmware comes from. The fork's apps are linked at 0x08004000 behind the
# OpenBLT CAN bootloader, so a blank board needs the bootloader too; official images are
# standalone at 0x08000000.
$SOURCES = [ordered]@{
    Stable   = @{ Repo = 'Coffee0297/CoffeeDingoFW'; Release = 'latest';              Bootloader = $true;  Label = 'CoffeeDingoFW stable (latest release)' }
    Testing  = @{ Repo = 'Coffee0297/CoffeeDingoFW'; Release = 'tags/testing-latest'; Bootloader = $true;  Label = 'CoffeeDingoFW testing (testing-latest build)' }
    Official = @{ Repo = 'corygrant/dingoFW';        Release = 'latest';              Bootloader = $false; Label = 'official dingoFW by corygrant (latest release)' }
}

# The release asset (.hex) for one board, e.g. 'canboard_v2' -> 'canboard_v2_FW_v5-5-108.hex'.
function Select-BoardAsset($Assets, [string]$Board) {
    return $Assets | Where-Object { $_.name -match ('^{0}_FW_.*\.hex$' -f [regex]::Escape($Board)) } | Select-Object -First 1
}

# The board marker every CoffeeDingoFW build after v5.5.108 carries in its image
# ('DINGO_BOARD=Dingo Electronics dingoPDM-Max v1'), or $null when the flash has none.
function Get-BoardMarker([byte[]]$Bytes) {
    $s = [Text.Encoding]::Latin1.GetString($Bytes)
    $i = $s.IndexOf('DINGO_BOARD=')
    if ($i -lt 0) { return $null }
    $end = $s.IndexOf([char]0, $i)
    if ($end -lt 0) { $end = $s.Length }
    return $s.Substring($i + 12, $end - $i - 12)
}

function Get-PdmVariant([string]$Marker) {
    if ($Marker -match 'PDM-Max') { return 'Max' }
    if ($Marker -match 'dingoPDM v7') { return 'V7' }
    return $null
}

# Image filename patterns of the two PDM variants. Bootloader images (dingopdm_blt) match neither,
# so they go to both.
$PDM_IMAGE = @{ V7 = 'dingopdm_v7'; Max = 'dingopdmmax' }

function Test-BothPdmVariants([string[]]$Images) {
    $names = @($Images | ForEach-Object { [IO.Path]::GetFileName(($_ -replace '@.*$', '')) })
    return ([bool]($names -match $PDM_IMAGE.V7)) -and ([bool]($names -match $PDM_IMAGE.Max))
}

function Select-PdmImages([string[]]$Images, [string]$Variant) {
    $other = if ($Variant -eq 'Max') { $PDM_IMAGE.V7 } else { $PDM_IMAGE.Max }
    return @($Images | Where-Object { [IO.Path]::GetFileName(($_ -replace '@.*$', '')) -notmatch $other })
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

    # --- USB DFU ---
    # A real F446 ROM bootloader alt name: 4x16K + 64K + 3x128K.
    if ((Get-DfuFlashSize '@Internal Flash  /0x08000000/04*016Kg,01*064Kg,03*128Kg') -ne 512KB) { throw 'SelfTest: F446 DFU layout not 512K' }
    if ($DFU_PARTS[(Get-DfuFlashSize '@Internal Flash  /0x08000000/04*016Kg,01*064Kg,03*128Kg')] -ne 0x421) { throw 'SelfTest: F446 DFU layout not mapped to the PDM' }
    if ((Get-DfuFlashSize '@Option Bytes  /0x1FFFC000/01*016 e') -ne 0) { throw 'SelfTest: non-flash alt read as flash' }
    # A mixed batch: each board gets its own images, plus ones that name no board.
    $mix = @($at0, $big, (Join-Path $dir 'shared.hex'))
    $forCb = @(Select-ImagesFor $f303 $mix)
    if ($forCb.Count -ne 2 -or $forCb -contains $big) { throw 'SelfTest: CANBoard got the wrong images from a mixed batch' }
    $forPdm = @(Select-ImagesFor $f446 $mix)
    if ($forPdm.Count -ne 2 -or $forPdm -contains $at0) { throw 'SelfTest: PDM got the wrong images from a mixed batch' }
    if (@(Select-ImagesFor $f446 @($at0)).Count -ne 1) { throw 'SelfTest: no-fit case must pass images on for the complaints' }
    # dfu-util writes one block: record gaps must be 0xFF, not 0x00.
    $gap = Join-Path $dir 'gap.hex'
    @(':020000040800F2', ':0400000001020304F2', ':0400080005060708DA', ':00000001FF') | Set-Content $gap
    $d = Get-DfuImage $gap
    if ($d.Start -ne 0x08000000 -or $d.Data.Length -ne 12) { throw "SelfTest: DFU block wrong ($($d.Start) / $($d.Data.Length))" }
    if ($d.Data[4] -ne 0xFF -or $d.Data[7] -ne 0xFF -or $d.Data[8] -ne 5) { throw 'SelfTest: DFU block gap not 0xFF' }
    $binSpec = "$gap@0x08004000"
    if ((Get-DfuImage $binSpec).Start -ne 0x08004000) { throw 'SelfTest: .bin@address start not parsed' }
    if ($null -ne (Get-DfuImage (Join-Path $dir 'x.elf'))) { throw 'SelfTest: .elf accepted for DFU' }

    # --- release download ---
    if (@($SOURCES.Keys)[0] -ne 'Stable') { throw 'SelfTest: Stable must be the first (default) source' }
    if (-not $SOURCES.Stable.Bootloader -or $SOURCES.Official.Bootloader) { throw 'SelfTest: bootloader flags wrong' }
    $assets = @('canboard_v2_FW_v5-5-108.elf', 'canboard_v2_FW_v5-5-108.hex', 'dingopdmmax_v1_FW_v5-5-108.hex',
                'dingopdm_v7_FW_v5-5-108.hex', 'pt-dpdm4_1_FW_v0-6-1.hex') | ForEach-Object { @{ name = $_ } }
    if ((Select-BoardAsset $assets 'canboard_v2').name -ne 'canboard_v2_FW_v5-5-108.hex') { throw 'SelfTest: canboard asset not picked' }
    if ((Select-BoardAsset $assets 'dingopdm_v7').name -ne 'dingopdm_v7_FW_v5-5-108.hex') { throw 'SelfTest: v7 asset not picked' }
    if ((Select-BoardAsset $assets 'dingopdmmax_v1').name -ne 'dingopdmmax_v1_FW_v5-5-108.hex') { throw 'SelfTest: Max asset not picked' }

    # --- telling a dingoPDM v7 from a -Max ---
    $fw = [Text.Encoding]::Latin1.GetBytes("junk`0DINGO_BOARD=Dingo Electronics dingoPDM-Max v1`0more")
    if ((Get-BoardMarker $fw) -ne 'Dingo Electronics dingoPDM-Max v1') { throw 'SelfTest: board marker not read' }
    if ((Get-PdmVariant (Get-BoardMarker $fw)) -ne 'Max') { throw 'SelfTest: Max marker not recognised' }
    if ((Get-PdmVariant 'Dingo Electronics dingoPDM v7') -ne 'V7') { throw 'SelfTest: v7 marker not recognised' }
    if ($null -ne (Get-BoardMarker ([byte[]]::new(64)))) { throw 'SelfTest: marker found in empty flash' }
    if ($null -ne (Get-PdmVariant 'Dingo Electronics CANBoard v2')) { throw 'SelfTest: CANBoard marker read as a PDM' }
    $rel = @('canboard_v2_FW_v5-5-108.hex', 'dingopdm_v7_FW_v5-5-108.hex', 'dingopdmmax_v1_FW_v5-5-108.hex', 'dingopdm_blt.hex')
    if (-not (Test-BothPdmVariants $rel)) { throw 'SelfTest: both PDM variants not seen' }
    if (Test-BothPdmVariants $rel[0..1]) { throw 'SelfTest: one variant read as both' }
    $forMax = Select-PdmImages $rel 'Max'
    if ($forMax -contains 'dingopdm_v7_FW_v5-5-108.hex' -or $forMax -notcontains 'dingopdm_blt.hex' -or $forMax -notcontains 'dingopdmmax_v1_FW_v5-5-108.hex') { throw 'SelfTest: wrong images for a Max' }
    $forV7 = Select-PdmImages $rel 'V7'
    if ($forV7 -contains 'dingopdmmax_v1_FW_v5-5-108.hex' -or $forV7 -notcontains 'dingopdm_v7_FW_v5-5-108.hex') { throw 'SelfTest: wrong images for a v7' }

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
        return @{ Unknown = $true; Via = 'swd'; Desc = ('DEV_ID 0x{0:X3}, rev 0x{1:X4}' -f $devId, ($idcode -shr 16)) }
    }
    $kb = Read-Word $p.FlashSizeReg
    $part = $p.Clone()
    $part.Via       = 'swd'
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


# --- USB DFU (ST ROM bootloader, 0483:df11) ---

# Boards sitting in the ROM bootloader: @{ Serial; Size } each, from alt 0 ('Internal Flash').
function Get-DfuDevices {
    if (-not $script:dfuUtil) { return @() }
    $devs = @()
    foreach ($l in (dfu-util -l 2>&1 | Out-String) -split "`n") {
        if ($l -match '\[0483:df11\].*\balt=0, name="([^"]*)", serial="([^"]*)"') {
            $serial = $Matches[2]   # Get-DfuFlashSize reuses $Matches
            $devs += @{ Serial = $serial; Size = (Get-DfuFlashSize $Matches[1]) }
        }
    }
    return $devs
}

# The first board in DFU, identified from its flash layout. $null when there is none.
function Get-DfuPart {
    $d = @(Get-DfuDevices) | Select-Object -First 1
    if (-not $d) { return $null }
    $devId = $DFU_PARTS[$d.Size]
    if ($null -eq $devId) {
        return @{ Unknown = $true; Via = 'usb'; Serial = $d.Serial; Desc = ('USB DFU device with {0} KB flash' -f ($d.Size / 1KB)) }
    }
    $part = $PARTS[$devId].Clone()
    $part.Via = 'usb'; $part.Serial = $d.Serial; $part.DevId = $devId; $part.Rev = 0; $part.FlashSize = $d.Size
    return $part
}

# Read $Size bytes of flash to $Path over DFU. Judged on the file, like Read-Flash.
function Read-FlashDfu([string]$Serial, [long]$Start, [int]$Size, [string]$Path) {
    Remove-Item $Path -ErrorAction SilentlyContinue   # dfu-util refuses to overwrite on upload
    dfu-util -d 0483:df11 -S $Serial -a 0 -s ("0x{0:X8}:{1}" -f $Start, $Size) -U $Path *> $null
    if (-not (Test-Path $Path)) { return $false }
    return (Get-Item $Path).Length -eq $Size
}

# Write each image at its own address. DfuSe erases every sector a download touches, the same
# scope as pyocd's '-e sector'. No ':leave', so the board stays in DFU for the read-back.
function Write-FlashDfu([string]$Serial, [string[]]$Images) {
    $bin = Join-Path ([IO.Path]::GetTempPath()) 'dingo-dfu.bin'
    foreach ($spec in $Images) {
        $img = Get-DfuImage $spec
        [IO.File]::WriteAllBytes($bin, $img.Data)
        Write-Host ("  writing {0} at 0x{1:X8} over USB..." -f [IO.Path]::GetFileName(($spec -replace '@.*$', '')), $img.Start)
        $out = dfu-util -d 0483:df11 -S $Serial -a 0 -s ("0x{0:X8}" -f $img.Start) -D $bin 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            ($out.Trim() -split "`n" | Select-Object -Last 3) | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
            return $false
        }
    }
    return $true
}

# Read $Size bytes of flash to $Path over whichever link the board is on. Returns the pyocd connect
# mode that worked ('dfu' over USB), or $null when the board can't be read.
function Read-Board($Part, [string]$TargetName, [long]$Start, [int]$Size, [string]$Path) {
    if ($Part.Via -eq 'usb') {
        if (Read-FlashDfu $Part.Serial $Start $Size $Path) { return 'dfu' }
        return $null
    }
    foreach ($m in $connectModes) {
        if (Read-Flash $TargetName $m $Start $Size $Path) { return $m }
    }
    return $null
}

# v7 or Max, asked when the board's firmware doesn't say. Enter repeats the last answer.
function Read-PdmVariant {
    $def = if ($script:lastPdm) { $script:lastPdm } else { 'V7' }
    $a = Read-Host "  Which board is this?  [1] dingoPDM v7   [2] dingoPDM-Max   (Enter = $def)"
    if ($a -match '^\s*$') { return $def }
    if ($a -match '^\s*(1|v7)\s*$') { return 'V7' }
    if ($a -match '^\s*(2|max)\s*$') { return 'Max' }
    return $null
}

function Test-StillConnected($Part) {
    if ($Part.Via -eq 'usb') { return [bool](@(Get-DfuDevices) | Where-Object { $_.Serial -eq $Part.Serial }) }
    return [bool](Get-ConnectedPart)
}

function Wait-Unplug($Part, [string]$Extra = '') {
    $how = if ($Part.Via -eq 'usb') { 'Unplug board (and release BOOT0)...' } else { 'Unplug board...' }
    Write-Host $how -NoNewline
    while (Test-StillConnected $Part) { Start-Sleep -Milliseconds 500 }
    Write-Host "`rBoard removed.$Extra                    `n"
}


# --- firmware download (no -Hex) ---

function Save-Url([string]$Url, [string]$Path, [switch]$Fresh) {
    if ($Fresh -or -not (Test-Path $Path)) {
        try { Invoke-WebRequest $Url -OutFile $Path -UseBasicParsing }
        catch { throw "Download failed: $Url ($($_.Exception.Message))" }
    }
    return $Path
}

# Download the CANBoard + PDM images of one release (and the bootloaders when the source needs
# them) into a cache folder, and return their paths for -Hex.
function Get-ReleaseImages([string]$Name) {
    $src = $SOURCES[$Name]
    $api = "https://api.github.com/repos/$($src.Repo)/releases/$($src.Release)"
    try { $rel = Invoke-RestMethod $api -Headers @{ 'User-Agent' = 'flash-dingo' } }
    catch { throw "No $Name release found on $($src.Repo) ($($api): $($_.Exception.Message))" }
    $pdms = if ($Pdm -eq 'Max') { @('dingopdmmax_v1') } elseif ($Pdm) { @('dingopdm_v7') } else { @('dingopdm_v7', 'dingopdmmax_v1') }
    $dir = Join-Path ([IO.Path]::GetTempPath()) ('dingo-firmware/{0}/{1}' -f ($src.Repo -replace '/', '_'), $rel.tag_name)
    $null = New-Item -ItemType Directory -Path $dir -Force
    Write-Host ("firmware: {0} -> {1}" -f $src.Label, $rel.tag_name) -ForegroundColor Cyan
    $files = @()
    foreach ($board in @('canboard_v2') + $pdms) {
        $a = Select-BoardAsset $rel.assets $board
        if (-not $a) { throw "$($src.Repo) $($rel.tag_name) has no .hex for $board" }
        # Asset names carry the version, so a cached file is the right one ('testing-latest' moves).
        $files += Save-Url $a.browser_download_url (Join-Path $dir $a.name)
    }
    if ($src.Bootloader) {
        # From the release (v5.5.109 on); older releases lack them, so fall back to master, fetched
        # fresh because master moves.
        foreach ($b in 'canboard', 'dingopdm') {
            $a = $rel.assets | Where-Object { $_.name -eq "$($b)_blt.hex" } | Select-Object -First 1
            if ($a) { $files += Save-Url $a.browser_download_url (Join-Path $dir $a.name) -Fresh }
            else {
                $url = "https://raw.githubusercontent.com/$($src.Repo)/master/bootloader/$b/bin/$($b)_blt.hex"
                $files += Save-Url $url (Join-Path $dir "$($b)_blt.hex") -Fresh
            }
        }
    }
    return $files
}

if (-not $Hex) {
    if (-not $Source) {
        if ($Yes) { $Source = 'Stable' }
        else {
            $names = @($SOURCES.Keys)
            Write-Host 'Firmware to flash:'
            for ($i = 0; $i -lt $names.Count; $i++) {
                $mark = if ($i -eq 0) { '  (default)' } else { '' }
                Write-Host ("  [{0}] {1}  -- {2}{3}" -f ($i + 1), $names[$i], $SOURCES[$names[$i]].Label, $mark)
            }
            $answer = Read-Host "Choice [1]"
            $pick = if ($answer -match '^\s*$') { 1 } elseif ($answer -match '^[1-9]$') { [int]$answer } else { 0 }
            if ($pick -lt 1 -or $pick -gt $names.Count) { Write-Host 'Aborted.'; exit 1 }
            $Source = $names[$pick - 1]
        }
    }
    $Hex = @(Get-ReleaseImages $Source)
}

foreach ($f in $Hex) {
    if (-not (Test-Path ($f -replace '@.*$', ''))) { throw "Missing image: $f" }
}

# Which links to watch. Auto uses whatever is there and says so; an explicit -Via insists.
$useSwd = $Via -ne 'Usb'
$useUsb = $Via -ne 'Swd'
if ($useUsb) {
    $script:dfuUtil = Get-Command dfu-util -ErrorAction SilentlyContinue
    if (-not $script:dfuUtil) {
        if ($Via -eq 'Usb') { throw 'dfu-util not found on PATH (needed for USB DFU).' }
        Write-Host 'USB DFU: off (dfu-util not on PATH)' -ForegroundColor DarkGray
        $useUsb = $false
    }
}
if ($useSwd) {
    $probe = (Get-Command pyocd -ErrorAction SilentlyContinue) -and (pyocd list 2>&1 | Select-String -Quiet 'Unique ID')
    if (-not $probe) {
        if ($Via -eq 'Swd') { throw 'No debug probe found by pyocd. Plug in the Debugprobe first.' }
        Write-Host 'SWD: off (no debug probe found)' -ForegroundColor DarkGray
        $useSwd = $false
    }
}
if (-not ($useSwd -or $useUsb)) { throw 'Neither a debug probe (SWD) nor dfu-util (USB DFU) is available.' }
Write-Host ('watching: {0}' -f ((@(if ($useSwd) { 'SWD probe' }) + @(if ($useUsb) { 'USB DFU' })) -join ' + '))

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
Write-Host ''

# Blank parts hardfault-loop on a 0xFFFFFFFF reset vector, so under-reset is tried first;
# 'halt' is the fallback for probes whose nRST is not wired through.
$connectModes = @('under-reset', 'halt')
$dump = Join-Path ([IO.Path]::GetTempPath()) 'dingo-blankcheck.bin'
$count = 0
$confirmed = @{}   # board name -> confirmed; one prompt per board type in a batch

while ($true) {
    Write-Host 'Waiting for board...' -NoNewline
    $part = $null
    while (-not $part) {
        # USB first: listing DFU devices is quick, while an absent SWD target costs a pass
        # through every fallback clock.
        if ($useUsb) { $part = Get-DfuPart }
        if (-not $part -and $useSwd -and (Set-WorkingClock)) { $part = Get-ConnectedPart }
        if (-not $part) { Start-Sleep -Milliseconds 500 }
    }

    if ($part.Unknown) {
        Write-Host ("`rUNKNOWN part ({0}) -- not a board this script knows." -f $part.Desc) -ForegroundColor Red
        [Console]::Beep(220, 400)
        if ($Once) { break }
        Wait-Unplug $part
        continue
    }

    $usb = $part.Via -eq 'usb'
    $tgt = if ($Target) { $Target } else { $part.Target }
    $link = if ($usb) { "USB DFU, serial $($part.Serial)" } else { "SWD, pyocd '$tgt'" }
    Write-Host ("`rConnected: {0}  ({1})  {2} KB flash  -> {3}" -f `
        $part.Name, $part.Board, ($part.FlashSize / 1024), $link) -ForegroundColor Cyan

    # --- which images, and do they belong on THIS board? ---
    $images = @(Select-ImagesFor $part $Hex)

    # Both PDM variants are an F446. Pick the image from -Pdm, else from the board marker in the
    # firmware already on it (an upgrade), else ask.
    if ($part.Keyword -eq 'dingopdm' -and ($Pdm -or (Test-BothPdmVariants $images))) {
        $variant = $Pdm
        if (-not $variant) {
            $peek = Get-ImagePlan $images
            $pFrom = if ($null -ne $peek.Start) { $peek.Start } else { 0x08000000 }
            $pSize = if ($null -ne $peek.Start) { [int]($peek.End - $peek.Start) } else { $part.FlashSize }
            if (Read-Board $part $tgt $pFrom $pSize $dump) {
                $marker = Get-BoardMarker ([IO.File]::ReadAllBytes($dump))
                $variant = Get-PdmVariant $marker
                if ($variant) { Write-Host "  firmware on the board says: $marker" }
            }
        }
        if (-not $variant) {
            if ($Yes) {
                Write-Host "SKIP  can't tell a dingoPDM v7 from a -Max on this board: pass -Pdm V7 or -Pdm Max" -ForegroundColor Yellow
                [Console]::Beep(330, 400)
                if ($Once) { break }
                Wait-Unplug $part
                continue
            }
            Write-Host "  No board marker in the firmware on it (blank, official or older firmware)."
            while (-not $variant) { $variant = Read-PdmVariant }
        }
        $script:lastPdm = $variant
        $images = Select-PdmImages $images $variant
    }
    $complaints = @(Get-ImageComplaints $part $images)
    if ($usb) {
        $complaints += @($images | Where-Object { $null -eq (Get-DfuImage $_) } |
            ForEach-Object { "'$([IO.Path]::GetFileName($_))' can't be written over USB DFU: use the .hex, or .bin@0x<address>" })
    }
    if ($complaints) {
        Write-Host 'MISMATCH between image and connected board:' -ForegroundColor Red
        $complaints | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
        if (-not $IgnoreMismatch) {
            Write-Host 'SKIP  refusing to flash. Use -IgnoreMismatch to override.' -ForegroundColor Red
            [Console]::Beep(220, 400)
            if ($Once) { break }
            Wait-Unplug $part
            continue
        }
        Write-Host '  -IgnoreMismatch given: proceeding anyway.' -ForegroundColor Yellow
    }

    # Blank-check and read back only the span the images occupy: reading a whole 512K F446 is
    # ~13 s of probe time versus ~4 s for the image, and bytes outside it are never written.
    $plan = Get-ImagePlan $images
    $from = if ($null -ne $plan.Start) { $plan.Start } else { 0x08000000 }
    $size = if ($null -ne $plan.Start) { [int]($plan.End - $plan.Start) } else { $part.FlashSize }
    Write-Host ("  image(s): {0}" -f (($images | ForEach-Object { [IO.Path]::GetFileName(($_ -replace '@.*$', '')) }) -join ', '))
    Write-Host ("  span:     0x{0:X8}-0x{1:X8} ({2:N0} KB)" -f $from, ($from + $size), ($size / 1024))

    # One confirmation per board type, once the first of that type has identified itself.
    if (-not $Yes -and -not $confirmed[$part.Board]) {
        $answer = Read-Host "Flash the image(s) above onto $($part.Board) boards? [y/N]"
        if ($answer -notmatch '^(y|yes)$') { Write-Host 'Aborted.'; exit 1 }
        $confirmed[$part.Board] = $true
    }

    $verify = $null
    try {
        # The flash readback is also the liveness check, so a marginal link cannot pass a probe
        # step and then fail the real one.
        $mode = Read-Board $part $tgt $from $size $dump
        if (-not $mode) {
            $hint = if ($usb) { 'replug with BOOT0 held, check the WinUSB driver (Zadig)' } else { 'reseat the board or lower -Frequency' }
            Write-Host "FAIL  could not read flash back -- $hint" -ForegroundColor Red
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
                    # Neither 'pyocd load' nor dfu-util reads the image back afterwards
                    # ('--trust-crc' only decides which pages to skip BEFORE writing), so the
                    # verify below is what actually confirms the board.
                    # Both erase every SECTOR an image touches, in full -- see the header.
                    if ($usb) { $ok = Write-FlashDfu $part.Serial $images }
                    else {
                        if ($Fast) { pyocd load -t $tgt -M $mode -f $script:clock -e sector --trust-crc @images }
                        else       { pyocd load -t $tgt -M $mode -f $script:clock -e sector @images }
                        $ok = $LASTEXITCODE -eq 0
                    }
                    if (-not $ok) {
                        Write-Host "  attempt $attempt/$Retries failed to program, retrying..." -ForegroundColor Yellow
                        continue
                    }
                    # --- read the image span back and compare it to the files, byte for byte ---
                    if ($null -eq $plan.Expected) {
                        Write-Host '  programmed, but image bytes are unknown (not a .hex) -- cannot verify' -ForegroundColor Yellow
                        $flashed = $true; break
                    }
                    $readOk = if ($usb) { Read-FlashDfu $part.Serial $from $size $dump } else { Read-Flash $tgt $mode $from $size $dump }
                    if (-not $readOk) {
                        Write-Host "  attempt $attempt/$Retries programmed but could not read back, retrying..." -ForegroundColor Yellow
                        continue
                    }
                    $v = Test-FlashAgainstImages $plan.Expected $from ([IO.File]::ReadAllBytes($dump))
                    if ($v.Ok) { $flashed = $true; $verify = $v; break }
                    Write-Host ("  attempt {0}/{1} VERIFY FAILED: {2:N0} of {3:N0} bytes wrong, first at 0x{4:X8}" -f `
                        $attempt, $Retries, $v.BadCount, $v.Checked, $v.FirstBad) -ForegroundColor Yellow
                }
                if ($flashed) {
                    $count++
                    $what = if ($verify) { "{0:N0} bytes verified" -f $verify.Checked } else { 'NOT verified' }
                    Write-Host ("PASS  {0} #{1} flashed over {2}, {3}, at {4:HH:mm:ss}" -f `
                        $part.Board, $count, $(if ($usb) { 'USB' } else { 'SWD' }), $what, (Get-Date)) -ForegroundColor Green
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
    Wait-Unplug $part " $count passed so far."
}
