param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v[0-9]+\.[0-9]+\.[0-9]+$')]
    [string] $Version,

    [Parameter(Mandatory = $true)]
    [string] $Platform,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$outputPath = Join-Path $OutputDirectory "PluginAnalyzer-$Version-$Platform.spdx.json"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$repository = if ($env:GITHUB_REPOSITORY) {
    "https://github.com/$($env:GITHUB_REPOSITORY)"
} else {
    'https://github.com/koto-thing/PluginAnalyzer'
}
$revision = if ($env:GITHUB_SHA) { $env:GITHUB_SHA } else { 'local' }
$namespaceVersion = $Version.TrimStart('v')

$document = [ordered]@{
    spdxVersion = 'SPDX-2.3'
    dataLicense = 'CC0-1.0'
    SPDXID = 'SPDXRef-DOCUMENT'
    name = "PluginAnalyzer-$Version-$Platform"
    documentNamespace = "$repository/releases/$namespaceVersion/sbom/$Platform/$revision"
    creationInfo = [ordered]@{
        created = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
        creators = @('Tool: PluginAnalyzer-GitHub-Actions')
    }
    packages = @(
        [ordered]@{
            name = 'PluginAnalyzer'
            SPDXID = 'SPDXRef-Package-PluginAnalyzer'
            versionInfo = $namespaceVersion
            downloadLocation = "$repository/tree/$revision"
            filesAnalyzed = $false
            licenseConcluded = 'NOASSERTION'
            licenseDeclared = 'MIT'
            copyrightText = 'NOASSERTION'
            primaryPackagePurpose = 'APPLICATION'
        },
        [ordered]@{
            name = 'JUCE'
            SPDXID = 'SPDXRef-Package-JUCE'
            versionInfo = '8.0.13'
            downloadLocation = 'https://github.com/juce-framework/JUCE/tree/8.0.13'
            filesAnalyzed = $false
            licenseConcluded = 'NOASSERTION'
            licenseDeclared = 'NOASSERTION'
            copyrightText = 'NOASSERTION'
            primaryPackagePurpose = 'LIBRARY'
        }
    )
    relationships = @(
        [ordered]@{
            spdxElementId = 'SPDXRef-DOCUMENT'
            relationshipType = 'DESCRIBES'
            relatedSpdxElement = 'SPDXRef-Package-PluginAnalyzer'
        },
        [ordered]@{
            spdxElementId = 'SPDXRef-Package-PluginAnalyzer'
            relationshipType = 'DEPENDS_ON'
            relatedSpdxElement = 'SPDXRef-Package-JUCE'
        }
    )
}

$document | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $outputPath -Encoding utf8NoBOM
Write-Host "Created $outputPath"

