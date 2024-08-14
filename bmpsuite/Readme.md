# BMP Test Image Files

This folder contains various variants of images in Windows Bitmap format.

## BMP Suite

The files are from the [BMP Suite](https://entropymine.com/jason/bmpsuite/) version 2.7.
The [source code](https://github.com/jsummers/bmpsuite) with which the files can be generated is available on GitHub.
The author, Jason Summers, also provides a [description of each image](https://entropymine.com/jason/bmpsuite/bmpsuite/html/bmpsuite.html).

## Additional Images

The BMP Suite is a great collection. However, some useful variants are missing (especially for Windows developers).
Therefore, the following additional images were created with a [modified](https://github.com/Electron-x/bmpsuite) bmpsuite.c:

`x/msvideo1.bmp`
`x/cinepak.bmp`

These are two video compressed bitmaps.
Windows does not provide a 64-bit version of the Cinepak codec. Therefore, the 32-bit (x86) application is required to display the Cinepak image.
FFmpeg was used to compress the bitmap bits. However, using the Video Compression Manager API, any application could generate such custom DIB formats.

`b/rgb24bf.bmp`

This is a 24-bit bitmap with masks and the BI_BITFIELDS flag set.
This combination is not specified, but could be useful for testing.

`b/pal8-zo.bmp`

This is a bitmap with bfOffBits = 0 set in the file header.
In early Software Development Kit samples, packed bitmap files were marked this way.

`b/nopalette.bmp`

The required color table is missing in this file. This case is tricky because a biClrUsed value of 0 indicates a complete color table.
In the case of a packed memory DIB, it is no longer possible to determine the start of the bitmap bits.

`q/pal8os2offs.bmp`

Like pal8offs.bmp, this file has a gap between the color table and the bitmap bits.
However, you cannot simply create a packed DIB from this OS/2 variant by adjusting biClrUsed.

`q/rgb16os2.bmp`
`q/rgb32os2.bmp`
`q/rgb64os2.bmp`

These are standard BI_RGB bitmaps that use a BITMAPCOREHEADER instead of a BITMAPINFOHEADER.
The bit depths are not specified for OS/2 1.1 bitmaps, but the 16-bit and 32-bit versions are supported by all Windows functions.

`g/rgb24calibr.bmp`

A calibrated v4 bitmap with linear tone mapping (gamma = 1.0) and with the chromaticity endpoints for red and green swapped.
An application that supports Image Color Management (ICM) displays this image with the correct colors.

`q/devicergb.bmp`

This v4 bitmap already contains colors that are adapted to the output device and has been marked accordingly.
All primaries are set to the same endpoints (results in a greyscale image) to see if they are ignored by the color manager.
This format was dropped shortly after its introduction.
Presumably because such device-dependent image data can also be saved with the old v3 structure.

`q/cmyk.bmp`

A v4 bitmap with device-dependent CMYK data (the RGB color values were simply inverted here).
All primaries are set to the same endpoints (results in a greyscale image) to see if they are ignored by the color manager.
This format was dropped shortly after its introduction.
Presumably because this representation of the image data can also be saved with the old v3 structure.
However, the BI_CMYK flag had to be introduced for this.

`q/rgb64.bmp`
`q/rgba64.bmp`
`q/rgb64fakealpha.bmp`
`q/rgba64imagclrs.bmp`

64-bpp bitmaps can be read and written using the Windows Imaging Component (WIC).
However, the Graphics Device Interface (GDI) does not support this format.
rgba64imagclrs.bmp contains imaginary colors outside the normalized [0, 1] range.
Version 2.8 of the BMP Suite now also contains a variant of this HDR format.
