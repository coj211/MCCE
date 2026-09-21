Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Shot {
    public delegate bool EnumProc(IntPtr h, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lp);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@
Add-Type -AssemblyName System.Drawing
$target = (Get-Process MinecraftWin32_GL -ErrorAction SilentlyContinue | Select-Object -First 1).Id
if (-not $target) { "no process"; exit }
$script:win = [IntPtr]::Zero
$cb = [Shot+EnumProc]{
    param($h, $lp)
    $p = 0
    [Shot]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
    if ($p -eq $target) {
        $sb = New-Object System.Text.StringBuilder 128
        [Shot]::GetClassName($h, $sb, 128) | Out-Null
        if ($sb.ToString() -eq "OGLES") { $script:win = $h }
    }
    return $true
}
[Shot]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($script:win -eq [IntPtr]::Zero) { "OGLES window not found"; exit }
[Shot]::SetForegroundWindow($script:win) | Out-Null
Start-Sleep -Milliseconds 500
$r = New-Object Shot+RECT
[Shot]::GetWindowRect($script:win, [ref]$r) | Out-Null
$w = $r.R - $r.L; $hgt = $r.B - $r.T
"window: $($r.L),$($r.T) ${w}x${hgt}"
$bmp = New-Object System.Drawing.Bitmap($w, $hgt)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size($w, $hgt)))
$out = "H:\workerspace\workapace\main\MinecraftPE-Win\tools\win_shot.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
"$out saved"
