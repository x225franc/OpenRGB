param([string]$InstalledRoot='C:\Program Files\OpenRGB',[switch]$Remove)
$ErrorActionPreference='Stop'
$taskName='OpenRGB LightSync Resume Recovery'
$service=New-Object -ComObject 'Schedule.Service'; $service.Connect(); $folder=$service.GetFolder('\')
$existing=$null;try{$existing=$folder.GetTask($taskName)}catch{}
if($existing -and $existing.Definition.RegistrationInfo.Description -ne 'Start OpenRGB only if stopped after Windows resume. Managed by OpenRGB LightSync.') {throw 'An unrelated task uses the recovery task name.'}
if($Remove){if($existing){$folder.DeleteTask($taskName,0)};Write-Output 'Resume recovery removed.';return}
$InstalledRoot=[IO.Path]::GetFullPath($InstalledRoot)
if(-not(Test-Path -LiteralPath (Join-Path $InstalledRoot 'OpenRGB.exe'))){throw 'OpenRGB installation not found.'}
$scriptDir=Join-Path $InstalledRoot 'scripts';New-Item -ItemType Directory -Path $scriptDir -Force | Out-Null
$source=Join-Path $PSScriptRoot 'Resume-OpenRGB.ps1';$target=Join-Path $scriptDir 'Resume-OpenRGB.ps1'
if([IO.Path]::GetFullPath($source) -ne [IO.Path]::GetFullPath($target)){Copy-Item -LiteralPath $source -Destination $target -Force}
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$command=[IO.Path]::Combine([Environment]::GetFolderPath('System'),'WindowsPowerShell\v1.0\powershell.exe')
$arguments='-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "'+$target+'"'
$subscription='<QueryList><Query Id="0" Path="System"><Select Path="System">*[System[Provider[@Name=''Microsoft-Windows-Power-Troubleshooter''] and EventID=1]]</Select></Query></QueryList>'
function Escape([string]$text){[Security.SecurityElement]::Escape($text)}
$xml=@"
<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
 <RegistrationInfo><Description>Start OpenRGB only if stopped after Windows resume. Managed by OpenRGB LightSync.</Description></RegistrationInfo>
 <Triggers><EventTrigger><Enabled>true</Enabled><Subscription>$(Escape $subscription)</Subscription><Delay>PT20S</Delay></EventTrigger></Triggers>
 <Principals><Principal id="User"><UserId>$sid</UserId><LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>
 <Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><StartWhenAvailable>false</StartWhenAvailable><Enabled>true</Enabled><Hidden>true</Hidden><WakeToRun>false</WakeToRun><ExecutionTimeLimit>PT1M</ExecutionTimeLimit></Settings>
 <Actions Context="User"><Exec><Command>$(Escape $command)</Command><Arguments>$(Escape $arguments)</Arguments></Exec></Actions>
</Task>
"@
$folder.RegisterTask($taskName,$xml,6,$sid,$null,3,$null) | Out-Null
Write-Output 'Resume recovery installed for the current interactive Windows user; 20-second delay, no duplicate process, no wake timer.'
