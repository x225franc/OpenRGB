param([Parameter(Mandatory=$true)][int]$ProcessId,
      [ValidateSet('prepare','static','red','green','blue','slider','close')][string]$Action='prepare')
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class RGBToolInput {
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 public delegate bool Callback(IntPtr w, IntPtr p);
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)] public struct Point {public int X,Y;}
 [DllImport("user32.dll")] public static extern bool EnumWindows(Callback cb,IntPtr p);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w,out Rect r);
 [DllImport("user32.dll",SetLastError=true)] public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w,ref Point p);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
}
'@
[void][RGBToolInput]::SetProcessDPIAware()
$target=Get-Process -Id $ProcessId
if($target.ProcessName -notlike 'AMD_Radeon_RX_7900_XTX_Series_RGB_LED*') {throw 'Unexpected target process'}
if(Get-Process -Name OpenRGB -ErrorAction SilentlyContinue) {throw 'Close OpenRGB first'}
$script:window=[IntPtr]::Zero
$script:area=0
$callback=[RGBToolInput+Callback]{param($w,$p)
 [uint32]$owner=0
 [void][RGBToolInput]::GetWindowThreadProcessId($w,[ref]$owner)
 $r=New-Object RGBToolInput+Rect
 if($owner -eq $ProcessId -and [RGBToolInput]::IsWindowVisible($w) -and [RGBToolInput]::GetClientRect($w,[ref]$r)) {
  $area=($r.Right-$r.Left)*($r.Bottom-$r.Top)
  if($area -gt $script:area) {$script:window=$w;$script:area=$area}
 }
 return $true
}
[void][RGBToolInput]::EnumWindows($callback,[IntPtr]::Zero)
if($script:window -eq [IntPtr]::Zero) {throw 'No visible RGB Tool window'}
$bounds=New-Object RGBToolInput+Rect
[void][RGBToolInput]::GetClientRect($script:window,[ref]$bounds)
$width=$bounds.Right-$bounds.Left;$height=$bounds.Bottom-$bounds.Top
if([Math]::Abs($width/$height-1536/864) -gt 0.02) {throw 'Unexpected window aspect ratio; inspect screenshot first'}
[void][RGBToolInput]::SetForegroundWindow($script:window)
Start-Sleep -Milliseconds 300
if([RGBToolInput]::GetForegroundWindow() -ne $script:window) {throw 'RGB Tool could not be made foreground; no input sent'}
function Send-Pointer([uint32]$message,[int]$x,[int]$y,[int]$buttons) {
 $px=[int]($x*$width/1536);$py=[int]($y*$height/864)
 if([RGBToolInput]::GetForegroundWindow() -ne $script:window) {throw 'Focus changed; input stopped'}
 $point=New-Object RGBToolInput+Point;$point.X=$px;$point.Y=$py
 if(-not [RGBToolInput]::ClientToScreen($script:window,[ref]$point)) {throw 'ClientToScreen failed'}
 if(-not [RGBToolInput]::SetCursorPos($point.X,$point.Y)) {throw 'SetCursorPos failed'}
 if($message -eq 0x201) {[RGBToolInput]::mouse_event(2,0,0,0,[UIntPtr]::Zero)}
 if($message -eq 0x202) {[RGBToolInput]::mouse_event(4,0,0,0,[UIntPtr]::Zero)}
}
function Click([int]$x,[int]$y) {
 Send-Pointer 0x200 $x $y 0
 Send-Pointer 0x201 $x $y 1
 Start-Sleep -Milliseconds 60
 Send-Pointer 0x202 $x $y 0
 Start-Sleep -Milliseconds 900
}
switch($Action) {
 'prepare' {
  Click 315 770 # Chroma off
  Click 1120 201 # Select Color
  Click 870 730 # Static
  Click 842 201 # Light On
  for($i=0;$i -lt 5;$i++) {Click 992 501} # Known brightness + button
 }
 'static' {Click 870 730}
 'red' {Click 974 286}
 'green' {Click 1040 286}
 'blue' {Click 1062 286}
 'slider' {
  Send-Pointer 0x201 1260 294 1
  for($i=0;$i -le 40;$i++) {Send-Pointer 0x200 (1260+[int](160*$i/40)) 294 1;Start-Sleep -Milliseconds 125}
  Send-Pointer 0x202 1420 294 0
 }
 'close' {if(-not [RGBToolInput]::PostMessage($script:window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)) {throw 'WM_CLOSE failed'}}
}
Write-Output "$Action sent to RGB Tool PID $ProcessId at $([DateTime]::UtcNow.ToString('o'))"
