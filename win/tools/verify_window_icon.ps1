# verify_window_icon.ps1 — launch the game, query the main window's ICON_SMALL
# via WM_GETICON, and sanity-check it is the grass block (green top / brown
# dirt bottom). Then kill the game.
$exe = 'H:\workerspace\workapace\main\MinecraftPE-Win\MinecraftWin32_GL.exe'
$wd  = 'H:\workerspace\workapace\main\MinecraftPE-Win'

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class WinIcon {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)]
  public static extern IntPtr FindWindowW(string cls, string title);
  [DllImport("user32.dll")]
  public static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  public const uint WM_GETICON = 0x7F;
  public static readonly IntPtr ICON_SMALL = new IntPtr(0);
  public static readonly IntPtr ICON_BIG = new IntPtr(1);
  [DllImport("user32.dll")] public static extern bool DestroyIcon(IntPtr hIcon);
}
"@

$proc = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds 9

$h = [WinIcon]::FindWindowW('OGLES', 'MinecraftPE Community Edition')
if ($h -eq [IntPtr]::Zero) {
  Write-Output 'RESULT: window not found'
  if (-not $proc.HasExited) { $proc.Kill() }
  exit 1
}
Write-Output "window hwnd=$h"

foreach ($which in @(@('ICON_SMALL', [WinIcon]::ICON_SMALL), @('ICON_BIG', [WinIcon]::ICON_BIG))) {
  $name = $which[0]
  $wparam = $which[1]
  $hicon = [WinIcon]::SendMessage($h, [WinIcon]::WM_GETICON, $wparam, [IntPtr]::Zero)
  if ($hicon -eq [IntPtr]::Zero) {
    Write-Output ("{0}: (null)" -f $name)
    continue
  }
  $icon = [System.Drawing.Icon]::FromHandle($hicon)
  $bmp = $icon.ToBitmap()
  $w = $bmp.Width; $hh = $bmp.Height
  $t = $bmp.GetPixel([int]($w / 2), [int]($hh * 0.15))
  $b = $bmp.GetPixel([int]($w / 2), [int]($hh * 0.85))
  $green = ($t.G -gt $t.R) -and ($t.G -gt $t.B) -and ($t.G -gt 60)
  $brown = ($b.R -gt $b.B) -and ($b.G -lt $b.R) -and ($b.R -gt 60)
  Write-Output ("{0}: {1}x{2} top=({3},{4},{5}) bottom=({6},{7},{8}) -> grass={9} dirt={10}" -f $name, $w, $hh, $t.R, $t.G, $t.B, $b.R, $b.G, $b.B, $green, $brown)
  $bmp.Dispose()
}

if (-not $proc.HasExited) { $proc.Kill() }
Write-Output 'game process terminated'
