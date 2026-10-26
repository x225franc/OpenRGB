param([Parameter(Mandatory=$true)][int]$ProcessId,[Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class OfficialWindowCapture {
    public delegate bool Callback(IntPtr window, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
}
'@
$script:selected=[IntPtr]::Zero
$script:largest=0
$callback=[OfficialWindowCapture+Callback]{param($window,$data)
    [uint32]$owner=0
    [void][OfficialWindowCapture]::GetWindowThreadProcessId($window,[ref]$owner)
    if($owner -eq $ProcessId -and [OfficialWindowCapture]::IsWindowVisible($window)) {
        $rect=New-Object OfficialWindowCapture+Rect
        [void][OfficialWindowCapture]::GetWindowRect($window,[ref]$rect)
        $area=($rect.Right-$rect.Left)*($rect.Bottom-$rect.Top)
        if($area -gt $script:largest) { $script:largest=$area; $script:selected=$window }
    }
    return $true
}
[void][OfficialWindowCapture]::EnumWindows($callback,[IntPtr]::Zero)
if($script:selected -eq [IntPtr]::Zero) { throw 'No visible official RGB Tool window found.' }
$bounds=New-Object OfficialWindowCapture+Rect
[void][OfficialWindowCapture]::GetWindowRect($script:selected,[ref]$bounds)
$bitmap=New-Object System.Drawing.Bitmap(($bounds.Right-$bounds.Left),($bounds.Bottom-$bounds.Top))
$graphics=[System.Drawing.Graphics]::FromImage($bitmap)
$dc=$graphics.GetHdc()
try {
    if(-not [OfficialWindowCapture]::PrintWindow($script:selected,$dc,2)) { throw 'PrintWindow failed; no desktop capture fallback used.' }
} finally { $graphics.ReleaseHdc($dc); $graphics.Dispose() }
try { $bitmap.Save([IO.Path]::GetFullPath($OutputPath),[System.Drawing.Imaging.ImageFormat]::Png) } finally { $bitmap.Dispose() }
Write-Output $OutputPath
