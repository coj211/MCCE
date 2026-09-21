param([int]$vk = 0x72)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  public static IntPtr Found = IntPtr.Zero;
  public static uint Target = 0;
  public static bool Cb(IntPtr h, IntPtr l) {
    uint pid; GetWindowThreadProcessId(h, out pid);
    if (pid == Target && IsWindowVisible(h)) { Found = h; return false; }
    return true;
  }
  public static IntPtr Find(uint pid) { Target = pid; Found = IntPtr.Zero; EnumWindows(Cb, IntPtr.Zero); return Found; }
}
"@
$p = Get-Process MinecraftWin32_GL -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Output "no-process"; exit 1 }
$h = [W]::Find([uint32]$p.Id)
Write-Output ("hwnd=" + $h)
if ($h -ne [IntPtr]::Zero) {
  [W]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]0) | Out-Null
  Start-Sleep -Milliseconds 60
  [W]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]0) | Out-Null
  Write-Output "key-sent"
}
