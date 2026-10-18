param([string]$BuildDirectory=(Join-Path $PSScriptRoot '..\build'))
$ErrorActionPreference='Stop'
$sourceRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$makefile=Get-Content -LiteralPath (Join-Path $BuildDirectory 'Makefile') -Raw
$target=[regex]::Match($makefile,'(?s)_intermediate_release\\\.obj\\startup\.obj:(.*?)(?:\r?\n\r?\n)')
if(-not $target.Success){throw 'startup.obj dependency rule was not found.'}
$header=[regex]::Match($target.Groups[1].Value,'[^\s"]*OpenRGBDialog\.h')
if(-not $header.Success){throw 'The startup object does not depend on OpenRGBDialog.h.'}
$resolved=[IO.Path]::GetFullPath((Join-Path $BuildDirectory $header.Value))
$expected=Join-Path $sourceRoot 'qt\OpenRGBDialog\OpenRGBDialog.h'
if($resolved -ne $expected){throw "The build resolved the window header outside this checkout: $resolved"}
$object=Join-Path $BuildDirectory '_intermediate_release\.obj\startup.obj'
if((Get-Item -LiteralPath $object).LastWriteTimeUtc -lt (Get-Item -LiteralPath $expected).LastWriteTimeUtc){throw 'startup.obj predates its window class header. Rebuild before delivering.'}
Write-Host 'Build isolation verified: startup.obj uses the current checkout window class and is up to date.'