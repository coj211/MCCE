Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Shot3 {
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
$cb = [Shot3+EnumProc]{
    param($h, $lp)
    $p = 0
    [Shot3]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
    if ($p -eq $target) {
        $sb = New-Object System.Text.StringBuilder 128
        [Shot3]::GetClassName($h, $sb, 128) | Out-Null
        if ($sb.ToString() -eq "OGLES") { $script:win = $h }
    }
    return $true
}
[Shot3]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($script:win -eq [IntPtr]::Zero) { "OGLES window not found"; exit }
$r = New-Object Shot3+RECT
[Shot3]::GetWindowRect($script:win, [ref]$r) | Out-Null
"win rect: L=$($r.L) T=$($r.T) R=$($r.R) B=$($r.B) W=$($r.R-$r.L) H=$($r.B-$r.T)"
[Shot3]::SetForegroundWindow($script:win) | Out-Null
Start-Sleep -Milliseconds 300
$w = $r.R - $r.L; $h = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $h))
$bmp.Save("H:\workerspace\workapace\main\MinecraftPE-Win\tools\shot.png")
"shot saved"
