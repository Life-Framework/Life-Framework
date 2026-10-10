# Install the GPU-box MCP servers and write Cursor MCP config for this checkout.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
node (Join-Path $PSScriptRoot "harness.mjs") setup
exit $LASTEXITCODE
