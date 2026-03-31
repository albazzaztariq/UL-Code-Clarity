$env:PATH = "C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
Set-Location (Join-Path $PSScriptRoot "build-mingw")
Start-Process ".\CodeClarity.exe"
