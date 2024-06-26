$ulrNupkgD3D12 = "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.613.3"
$ulrNupkgDXC   = "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.DXC/1.8.2403.24"

$myTempFolder = "$($ENV:Temp)/rbmkp4800.XEngine-Externals-temp"


Function Get-RedirectedUrl($url) {
    $response = [System.Net.HttpWebRequest]::Create($url).GetResponse()
    $response.ResponseUri.AbsoluteUri
    $response.Close()
}

Function DownloadFile($url, $destFolder) {
    $redirectedUrl = Get-RedirectedUrl $url
    $filename = Split-Path -Leaf $redirectedUrl
    $output = Join-Path $destFolder $filename
    Write-Host "Downloading $($filename)"
    [void](New-Item -ItemType Directory -Force -Path $destFolder)
    Invoke-WebRequest -Uri $redirectedUrl -OutFile $output
    $output
}

Function UnzipNupkg($pathNupkg) {
    $pathZip = "$($pathNupkg).zip"
    $pathUnzip = "$($pathNupkg).unzip"
    Write-Host "Extracting $(Split-Path -Leaf $pathNupkg)"
    Move-Item $pathNupkg -Destination $pathZip -Force
    Expand-Archive $pathZip -DestinationPath $pathUnzip -Force
    $pathUnzip
}

Remove-Item "Externals" -Force -Recurse -ErrorAction SilentlyContinue
Remove-Item $myTempFolder -Force -Recurse -ErrorAction SilentlyContinue

$pathUnzipD3D12 = UnzipNupkg (DownloadFile $ulrNupkgD3D12 $myTempFolder)
Copy-Item (Join-Path $pathUnzipD3D12 "build/native/bin/x64")    -Destination "Externals/Microsoft.Direct3D.D3D12/bin/x64" -Force -Recurse
Copy-Item (Join-Path $pathUnzipD3D12 "build/native/include")    -Destination "Externals/Microsoft.Direct3D.D3D12/include" -Force -Recurse

$pathUnzipDXC = UnzipNupkg (DownloadFile $ulrNupkgDXC $myTempFolder)
Copy-Item (Join-Path $pathUnzipDXC "build/native/bin/x64")  -Destination "Externals/Microsoft.Direct3D.DXC/bin/x64" -Force -Recurse
Copy-Item (Join-Path $pathUnzipDXC "build/native/lib/x64")  -Destination "Externals/Microsoft.Direct3D.DXC/lib/x64" -Force -Recurse
Copy-Item (Join-Path $pathUnzipDXC "build/native/include")  -Destination "Externals/Microsoft.Direct3D.DXC/include" -Force -Recurse

Remove-Item $myTempFolder -Force -Recurse

Write-Host "Press any key to continue..."
$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
