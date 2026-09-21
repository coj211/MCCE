Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class WinEnum2 {
    public delegate bool EnumProc(IntPtr h, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lp);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
}
"@
$proc = Get-Process MinecraftWin32_GL -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { "no process"; exit }
$target = $proc.Id
$script:rows = New-Object System.Collections.ArrayList
$cb = [WinEnum2+EnumProc]{
    param($h, $lp)
    $p = 0
    [WinEnum2]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
    if ($p -eq $target) {
        $sb = New-Object System.Text.StringBuilder 512
        $cb2 = New-Object System.Text.StringBuilder 128
        [WinEnum2]::GetWindowText($h, $sb, 512) | Out-Null
        [WinEnum2]::GetClassName($h, $cb2, 128) | Out-Null
        $null = $script:rows.Add(("0x{0:X} visible={1} class='{2}' title='{3}'" -f $h.ToInt64(), [WinEnum2]::IsWindowVisible($h), $cb2.ToString(), $sb.ToString()))
    }
    return $true
}
[WinEnum2]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
$script:rows
Write-Output ("done pid=" + $target)
