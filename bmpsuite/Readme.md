# BMP Test Image Files

This folder contains various variants of images in Windows Bitmap format.

## BMP Suite

The files are from the [BMP Suite](https://entropymine.com/jason/bmpsuite/) version 2.7.
Here on GitHub you can also find the [source code](https://github.com/jsummers/bmpsuite) with which the files can be generated.
The author, Jason Summers, also provides a [description of each image](https://entropymine.com/jason/bmpsuite/bmpsuite/html/bmpsuite.html).

## Additional Images

The BMP Suite is a great collection. However, some variants are missing. Therefore, the following files have been added using a modified `bmpsuite.c`:

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
In early Software Development Kit (SDK) samples, packed bitmap files were marked this way.

`q/rgb16os2.bmp`
`q/rgb32os2.bmp`
`q/rgb64os2.bmp`

These are standard BI_RGB bitmaps that use a BITMAPCOREHEADER instead of a BITMAPINFOHEADER.
The bit depths are not specified for OS/2 1.1 bitmaps, but the 16-bit and 32-bit versions are supported by all Windows functions.

`g/rgb24calibr.bmp`

A bitmap calibrated according to the ICM 1.0 standard using linear tone mapping and red/green swapped.
An application that supports Image Color Management (ICM) displays this image like a standard sRGB bitmap.

`q/devicergb.bmp`

This bitmap already contains colors that are adapted to the output device and has been marked accordingly.
All primaries are set to the same endpoints (results in a greyscale image) to see if they are ignored by the CMM.
After the introduction of ICM 2.0, the definition of this format was removed from later SDKs.

`q/cmyk.bmp`

ICM 1.0 specifies bitmaps containing CMYK data. The RGB color values were simply inverted here.
Again, the Color Management Module (CMM) must pass the data directly to the output device.
After the introduction of ICM 2.0, the definition of this format was also removed from later SDKs.

`q/rgb64.bmp`
`q/rgba64.bmp`
`q/rgb64fakealpha.bmp`
`q/rgba64imagclrs.bmp`

64-bpp bitmaps can be read and written using the Windows Imaging Component (WIC).
However, the Graphics Device Interface (GDI) does not support this format.
rgba64imagclrs.bmp contains imaginary colors outside the normalized [0, 1] range.
Version 2.8 of the BMP Suite now also contains a variant of this HDR format.
