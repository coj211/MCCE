# verify_exe_icon.ps1 — extract the icon embedded in the exe and sanity-check
# its pixels (grass block: green top half, brown dirt bottom half).
param(
  [string]$Exe = 'H:\workerspace\workapace\main\MinecraftPE-Win\MinecraftWin32_GL.exe',
  [string]$OutPng = 'H:\workerspace\workapace\main\MinecraftPE-Win\tools\icon_from_exe.png'
)
Add-Type -AssemblyName System.Drawing
$icon = [System.Drawing.Icon]::ExtractAssociatedIcon($Exe)
if ($null -eq $icon) { Write-Output 'RESULT: NO-ICON'; exit 1 }
$bmp = $icon.ToBitmap()
$w = $bmp.Width; $h = $bmp.Height
Write-Output ("icon size: {0}x{1}" -f $w, $h)
if ($w -eq 0 -or $h -eq 0) { Write-Output 'RESULT: EMPTY'; exit 1 }
# sample: top row (grass) vs bottom row (dirt)
$t = $bmp.GetPixel([int]($w / 2), [int]($h * 0.15))
$m = $bmp.GetPixel([int]($w / 2), [int]($h * 0.5))
$b = $bmp.GetPixel([int]($w / 2), [int]($h * 0.85))
Write-Output ("top   : R={0} G={1} B={2}" -f $t.R, $t.G, $t.B)
Write-Output ("middle: R={0} G={1} B={2}" -f $m.R, $m.G, $m.B)
Write-Output ("bottom: R={0} G={1} B={2}" -f $b.R, $b.G, $b.B)
$grassGreen = ($t.G -gt $t.R) -and ($t.G -gt $t.B) -and ($t.G -gt 60)
$dirtBrown  = ($b.R -gt $b.B) -and ($b.G -lt $b.R) -and ($b.R -gt 60)
Write-Output ("top is green-ish: {0} | bottom is brown-ish: {1}" -f $grassGreen, $dirtBrown)
if ($grassGreen -and $dirtBrown) {
  Write-Output 'RESULT: OK-grass-block-icon'
} else {
  Write-Output 'RESULT: UNEXPECTED-pixels'
  exit 2
}
$bmp.Save($OutPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output ("saved preview: {0}" -f $OutPng)
