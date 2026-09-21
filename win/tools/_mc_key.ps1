param([string]$text = "", [int]$key = 0, [switch]$enter)
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class KT {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
$p = Get-Process MinecraftWin32_GL -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Output "no-process"; exit 1 }
$script:win = [IntPtr]::Zero
$script:tid = [uint32]$p.Id
$cb = [KT+EnumProc]{
  param($h, $l)
  $other = 0
  [KT]::GetWindowThreadProcessId($h, [ref]$other) | Out-Null
  if ($other -eq $script:tid) {
     $sb = New-Object System.Text.StringBuilder 64
     [KT]::GetClassName($h, $sb, 64) | Out-Null
     if ($sb.ToString() -eq 'OGLES') { $script:win = $h; return $false }
  }
  return $true
}
[KT]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($script:win -eq [IntPtr]::Zero) { Write-Output "window not found"; exit 1 }
$h = $script:win
$scan = 0x001C0001
$n = $text.Length
if ($key -ne 0) {
  [KT]::PostMessage($h, 0x0100, [IntPtr]$key, [IntPtr]$scan) | Out-Null
  Start-Sleep -Milliseconds 60
  [KT]::PostMessage($h, 0x0101, [IntPtr]$key, [IntPtr](0xC0000000 -bor $scan)) | Out-Null
  Start-Sleep -Milliseconds 250
}
foreach ($ch in $text.ToCharArray()) {
  [KT]::PostMessage($h, 0x0102, [IntPtr][int]$ch, [IntPtr]1) | Out-Null
  Start-Sleep -Milliseconds 35
}
if ($enter) {
  Start-Sleep -Milliseconds 120
  [KT]::PostMessage($h, 0x0100, [IntPtr]0x0D, [IntPtr]$scan) | Out-Null
  Start-Sleep -Milliseconds 80
  [KT]::PostMessage($h, 0x0101, [IntPtr]0x0D, [IntPtr](0xC0000000 -bor $scan)) | Out-Null
}
Write-Output ("sent: key=" + $key + " text='" + $text + "' enter=" + $enter)
