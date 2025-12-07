if ($PSCommandPath) {
    Push-Location $PSCommandPath/../..
}

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$VS = If ($Env:VisualStudio) {$Env:VisualStudio} Else {'C:\Program Files\Microsoft Visual Studio\2022\Community'}

& "$VS\Common7\Tools\Launch-VsDevShell.ps1" -HostArch amd64 -Arch amd64 -SkipAutomaticLocation

conan install . --deployer=runtime_deploy --deployer-folder=build/Release/bin --build=missing -c:a tools.cmake.cmaketoolchain:generator=Ninja
cmake --preset conan-release
ninja -C build/Release

Pop-Location