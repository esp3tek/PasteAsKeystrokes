Add-Type -AssemblyName System.Drawing

function New-KeycapBitmap($size, $letter) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode    = [System.Drawing.Drawing2D.SmoothingMode]::None
    $g.PixelOffsetMode  = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $g.TextRenderingHint = if ($size -ge 24) {
        [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit
    } else {
        [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixel
    }

    $face  = [System.Drawing.Color]::FromArgb(255, 192, 192, 192)
    $white = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)
    $dark  = [System.Drawing.Color]::FromArgb(255, 128, 128, 128)
    $black = [System.Drawing.Color]::FromArgb(255,   0,   0,   0)

    $g.FillRectangle((New-Object System.Drawing.SolidBrush($face)), 0, 0, $size, $size)

    $whitePen = New-Object System.Drawing.Pen($white, 1)
    $blackPen = New-Object System.Drawing.Pen($black, 1)
    $darkPen  = New-Object System.Drawing.Pen($dark, 1)

    $g.DrawLine($whitePen, 0, $size - 2, 0, 0)
    $g.DrawLine($whitePen, 0, 0, $size - 1, 0)
    $g.DrawLine($blackPen, $size - 1, 0, $size - 1, $size - 1)
    $g.DrawLine($blackPen, 0, $size - 1, $size - 1, $size - 1)

    if ($size -ge 24) {
        $g.DrawLine($darkPen, 1, $size - 2, $size - 2, $size - 2)
        $g.DrawLine($darkPen, $size - 2, 1, $size - 2, $size - 1)
    }

    $fontSize = [int]([Math]::Max(6, $size * 0.5))
    $font = New-Object System.Drawing.Font("Tahoma", $fontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment     = [System.Drawing.StringAlignment]::Center
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $rect = New-Object System.Drawing.RectangleF(0, 0, $size, $size)
    $g.DrawString($letter, $font, (New-Object System.Drawing.SolidBrush($black)), $rect, $sf)

    $font.Dispose()
    $g.Dispose()
    return $bmp
}

$sizes = @(16, 24, 32, 48, 64, 256)
$bitmaps = @()
foreach ($s in $sizes) {
    $bitmaps += New-KeycapBitmap $s "V"
}

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)

$bw.Write([uint16]0)
$bw.Write([uint16]1)
$bw.Write([uint16]$sizes.Count)

$imageData = @()
foreach ($bmp in $bitmaps) {
    $ims = New-Object System.IO.MemoryStream
    $bmp.Save($ims, [System.Drawing.Imaging.ImageFormat]::Png)
    $imageData += ,$ims.ToArray()
}

$headerSize = 6 + 16 * $sizes.Count
$offsets    = @()
$cursor     = $headerSize
foreach ($d in $imageData) {
    $offsets += $cursor
    $cursor += $d.Length
}

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $sb = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([byte]$sb)
    $bw.Write([byte]$sb)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]$imageData[$i].Length)
    $bw.Write([uint32]$offsets[$i])
}

foreach ($d in $imageData) {
    $bw.Write($d)
}

$out = Join-Path $PSScriptRoot 'icon.ico'
[IO.File]::WriteAllBytes($out, $ms.ToArray())
foreach ($bmp in $bitmaps) { $bmp.Dispose() }
Write-Host "Wrote $out ($($ms.Length) bytes, $($sizes.Count) sizes: $($sizes -join ', '))"
