param([Parameter(Mandatory=$true)][string]$Backup,[switch]$RestoreConfiguration)
$ErrorActionPreference='Stop'
if(Get-Process OpenRGB -ErrorAction SilentlyContinue){throw 'Quittez OpenRGB avant restauration.'}
$state=Get-Content (Join-Path $Backup 'rollback-modern.json') -Raw|ConvertFrom-Json
foreach($relative in $state.Files){$dest=[IO.Path]::GetFullPath((Join-Path $state.Target $relative));if(-not $dest.StartsWith(([IO.Path]::GetFullPath($state.Target).TrimEnd('\')+'\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Chemin de restauration invalide.'};if($state.Existing -contains $relative){Copy-Item -LiteralPath (Join-Path $Backup $relative) -Destination $dest -Force}elseif(Test-Path -LiteralPath $dest){Remove-Item -LiteralPath $dest}}
if($RestoreConfiguration){Copy-Item -Path (Join-Path $Backup 'config\*') -Destination (Join-Path $state.Target 'config') -Recurse -Force}
& (Join-Path $PSScriptRoot 'scripts\Register-ApplicationIdentity.ps1') -AppRoot $state.Target -BackupRoot (Join-Path $Backup 'identity-restoration')
Write-Output 'Anciens binaires restaures, identite Windows conservee.'
