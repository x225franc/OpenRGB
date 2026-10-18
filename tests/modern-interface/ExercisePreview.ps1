param(
    [string]$ReviewDirectory=(Join-Path $PSScriptRoot '..\..\build\ui-review'),
    [int]$Iterations=160
)
$ErrorActionPreference='Stop'
$ReviewDirectory=[IO.Path]::GetFullPath($ReviewDirectory)
if(-not(Test-Path -LiteralPath $ReviewDirectory)){throw 'Start the explicitly enabled development review module first.'}
$measurements=New-Object Collections.Generic.List[object]
function Invoke-Review([hashtable]$Command){
    $Command.id=[Guid]::NewGuid().ToString('N')
    $temporary=Join-Path $ReviewDirectory 'exercise.tmp'
    $Command|ConvertTo-Json|Set-Content -LiteralPath $temporary -Encoding utf8
    $timer=[Diagnostics.Stopwatch]::StartNew()
    Move-Item -LiteralPath $temporary -Destination (Join-Path $ReviewDirectory 'command.json') -Force
    do {
        Start-Sleep -Milliseconds 60
        try {$reply=Get-Content (Join-Path $ReviewDirectory 'response.json') -Raw -Encoding utf8|ConvertFrom-Json}catch{continue}
        if($reply.id -eq $Command.id){
            if(-not $reply.ok){throw "Review failed: $($Command.action): $($reply.error)"}
            $measurements.Add(@{Action=$Command.action;RoundTripMilliseconds=$timer.Elapsed.TotalMilliseconds})
            return $reply
        }
    }while($timer.Elapsed.TotalSeconds -lt 12)
    throw "No response: $($Command.action)"
}
$started=(Get-Date).ToUniversalTime().ToString('o')
$pages=@('Devices','Effects','LightSync','Visual Map','SDK Server','SDK Client','Settings','Information')
$widths=@(1080,1280,1440,1920)
$heights=@(720,920,1080)
$checks=0
try {
    Invoke-Review @{action='show'}|Out-Null
    for($i=0;$i -lt $Iterations;$i++){
        $width=$widths[[int][Math]::Floor($i/$pages.Count)%$widths.Count]
        $height=$heights[[int][Math]::Floor($i/($pages.Count*$widths.Count))%$heights.Count]
        Invoke-Review @{action='resize';width=$width;height=$height}|Out-Null
        $page=$pages[$i%$pages.Count]
        Invoke-Review @{action='navigate';page=$page}|Out-Null
        $state=Invoke-Review @{action='state'}
        $heading=@($state.widgets|Where-Object name -eq 'PageTitle')
        if($heading.Count -ne 1 -or $heading[0].text -ne $page){throw "Incorrect selected page: $page"}
        $bar=@($state.widgets|Where-Object name -eq 'ProfileCommandBar')
        if($bar.Count -ne 1 -or $bar[0].y+$bar[0].h -gt $state.height -or $bar[0].x+$bar[0].w -gt $state.width){throw 'Profile commands are outside the window.'}
        $side=@($state.widgets|Where-Object name -eq 'AppSidebar')
        $expected=if($width -lt 1440){88}else{248}
        if($side.Count -ne 1 -or $side[0].w -ne $expected){throw 'Responsive sidebar size mismatch.'}
        if($page -eq 'Devices'){
            Invoke-Review @{action='search';text='Lenovo'}|Out-Null
            Invoke-Review @{action='search';text=''}|Out-Null
        }
        $checks+=3
    }
    Invoke-Review @{action='resize';width=1440;height=920}|Out-Null
    Invoke-Review @{action='navigate';page='Devices'}|Out-Null
    Invoke-Review @{action='hide'}|Out-Null
    $result=@{Passed=$true;Started=$started;Completed=(Get-Date).ToUniversalTime().ToString('o');Iterations=$Iterations;Assertions=$checks;Commands=$measurements.ToArray()}
}catch{
    $result=@{Passed=$false;Started=$started;Completed=(Get-Date).ToUniversalTime().ToString('o');Assertions=$checks;Error=($_|Out-String);Commands=$measurements.ToArray()}
    throw
}finally{
    $result|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $ReviewDirectory 'exercise-result.json') -Encoding utf8
}
$result|Select-Object Passed,Started,Completed,Iterations,Assertions