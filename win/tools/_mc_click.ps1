param([int]$x = -1, [int]$y = -1, [int]$n = 1, [int]$delay = 500, [switch]$rect)
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Clk {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@
$p = Get-Process MinecraftWin32_GL -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { "no-process"; exit 1 }
$script:win = [IntPtr]::Zero
$script:tid = [uint32]$p.Id
$cb = [Clk+EnumProc]{
  param($h, $l)
  $other = 0
  [Clk]::GetWindowThreadProcessId($h, [ref]$other) | Out-Null
  if ($other -eq $script:tid) {
     $sb = New-Object System.Text.StringBuilder 64
     [Clk]::GetClassName($h, $sb, 64) | Out-Null
     if ($sb.ToString() -eq 'OGLES') { $script:win = $h; return $false }
  }
  return $true
}
[Clk]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($script:win -eq [IntPtr]::Zero) { "window not found"; exit 1 }
$r = New-Object Clk+RECT
[Clk]::GetClientRect($script:win, [ref]$r) | Out-Null
Write-Output ("client W=" + $r.R + " H=" + $r.B)
if ($rect) { exit 0 }
if ($x -lt 0 -or $y -lt 0) { exit 0 }
$lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
for ($i = 0; $i -lt $n; $i++) {
  [Clk]::PostMessage($script:win, 0x0200, [IntPtr]0, $lp) | Out-Null
  Start-Sleep -Milliseconds 120
  [Clk]::PostMessage($script:win, 0x0201, [IntPtr]1, $lp) | Out-Null
  Start-Sleep -Milliseconds 90
  [Clk]::PostMessage($script:win, 0x0202, [IntPtr]0, $lp) | Out-Null
  Start-Sleep -Milliseconds $delay
}
Write-Output ("clicked " + $n + " at " + $x + "," + $y)
