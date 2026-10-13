$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) {
        throw 'emcc was not found. Install and activate Emscripten: https://emscripten.org/docs/getting_started/downloads.html'
    }
    emcc wasm/minifs_wasm.c -std=c11 -O2 -Wall -Wextra -o web/minifs-c.js `
        -s MODULARIZE=1 `
        -s EXPORT_NAME=MiniFSCModule `
        -s ENVIRONMENT=web `
        -s ALLOW_MEMORY_GROWTH=1 `
        -s FILESYSTEM=1 `
        -s ASSERTIONS=1 `
        -s 'EXPORTED_FUNCTIONS=["_minifs_init","_minifs_execute","_minifs_export_tree_json","_minifs_get_cwd","_minifs_free","_malloc","_free"]' `
        -s 'EXPORTED_RUNTIME_METHODS=["ccall","cwrap","UTF8ToString"]'
    if ($LASTEXITCODE -ne 0) { throw "emcc failed with exit code $LASTEXITCODE" }
    Write-Host 'Built web/minifs-c.js and web/minifs-c.wasm'
}
finally { Pop-Location }
