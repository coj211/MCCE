# verify_window_title.ps1 — launch the game and confirm the top-level window
# title is exactly "MinecraftPE Community Edition".
$exe = 'H:\workerspace\workapace\main\MinecraftPE-Win\MinecraftWin32_GL.exe'
$wd  = 'H:\workerspace\workapace\main\MinecraftPE-Win'

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class WinEnum2 {
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern IntPtr FindWindowW(string cls, string title);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)]
  public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
}
"@

$proc = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds 9

# 1) exact match by title
$h = [WinEnum2]::FindWindowW($null, 'MinecraftPE Community Edition')
if ($h -ne [IntPtr]::Zero) {
  Write-Output "RESULT: OK exact title found hwnd=$h"
} else {
  Write-Output 'RESULT: exact FindWindowW miss — enumerating visible windows:'
  $all = @()
  $cb = [WinEnum2+EnumProc]{
    param($hw, $lp)
    $sb = New-Object System.Text.StringBuilder 512
    [WinEnum2]::GetWindowTextW($hw, $sb, 512) | Out-Null
    if ($sb.Length -gt 0 -and [WinEnum2]::IsWindowVisible($hw)) {
      $script:all += ("[{0}] {1}" -f $hw, $sb.ToString())
    }
    return $true
  }
  [WinEnum2]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
  $script:all | ForEach-Object { Write-Output $_ }
  Write-Output 'RESULT: NOT-FOUND'
}

$proc.Kill()
Write-Output 'game process terminated'
