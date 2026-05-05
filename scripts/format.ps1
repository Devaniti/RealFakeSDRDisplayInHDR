Push-Location $PSScriptRoot
Get-ChildItem -Path ../src -Include ('*.cpp','*.h','*.hpp') -File -Recurse | 
    ForEach-Object -Parallel {
        & clang-format -i $_.FullName
    }
Pop-Location
