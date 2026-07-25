param(
  [Parameter(Mandatory = $true)]
  [string]$HeaderPath
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $HeaderPath)) {
  Write-Output '1.0.0'
  exit 0
}

$text = Get-Content -Raw -LiteralPath $HeaderPath
# Prefer AppVersion() definition; fall back to first QStringLiteral("x.y.z")
$m = [regex]::Match($text, 'AppVersion\s*\(\)\s*\{[^}]*QStringLiteral\("([0-9]+(?:\.[0-9]+)*)"\)')
if (-not $m.Success) {
  $m = [regex]::Match($text, 'QStringLiteral\("([0-9]+(?:\.[0-9]+)*)"\)')
}
if ($m.Success) {
  Write-Output $m.Groups[1].Value
} else {
  Write-Output '1.0.0'
}
