$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    $fqbn = "m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=disabled"
    & arduino-cli compile --jobs 2 --fqbn $fqbn --warnings all --output-dir "$PSScriptRoot/build" "$PSScriptRoot/goputer_ADV"
    if ($LASTEXITCODE -ne 0) { throw "Arduino build failed ($LASTEXITCODE)" }
    New-Item -ItemType Directory -Force dist | Out-Null
    Copy-Item -Force build/goputer_ADV.ino.bin dist/goputer_ADV.bin
} finally { Pop-Location }
