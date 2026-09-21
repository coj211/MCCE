# list_game_windows.ps1 — launch the game, list every top-level window owned by
# the game process (class name + title), then kill it.
$exe = 'H:\workerspace\workapace\main\MinecraftPE-Win\MinecraftWin32_GL.exe'
$wd  = 'H:\workerspace\workapace\main\MinecraftPE-Win'

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class WinEnum3 {
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)]
  public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)]
  public static extern int GetClassNameW(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
}
"@

$proc = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds 9

$rows = @()
$cb = [WinEnum3+EnumProc]{
  param($hw, $lp)
  $u = 0
  $pid_ = [WinEnum3]::GetWindowThreadProcessId($hw, [ref]$u)
  if ($u -eq $script:targetPid) {
    $sb = New-Object System.Text.StringBuilder 512
    $sc = New-Object System.Text.StringBuilder 256
    [WinEnum3]::GetWindowTextW($hw, $sb, 512) | Out-Null
    [WinEnum3]::GetClassNameW($hw, $sc, 256) | Out-Null
    $script:rows += ("hwnd={0} visible={1} class=[{2}] title=[{3}]" -f $hw, [WinEnum3]::IsWindowVisible($hw), $sc.ToString(), $sb.ToString())
  }
  return $true
}
$script:targetPid = $proc.Id
[WinEnum3]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null

Write-Output ("game pid: {0}" -f $proc.Id)
if ($rows.Count -eq 0) { Write-Output 'RESULT: no top-level windows found for game pid' }
else { $rows | ForEach-Object { Write-Output $_ } }

if (-not $proc.HasExited) { $proc.Kill() }
Write-Output 'game process terminated'
