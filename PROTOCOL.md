
# Data protocol

This document describes the data protocol used for screen/video/audio data

## Packet types
 01 - cursor position
 02 - screen update

n. bytes | type   | description
-------- | ------ | ------------
       1 | uint8  | type of packet
       ? | data   | contents of packet

## cursor position

n. bytes | type   | description
-------- | ------ | ------------
       2 | uint16 | x position of cursor (network byte order)
       2 | uint16 | y position of cursor (network byte order)
       1 | uint8  | cursor type

cursor type denotes which cursor should be used when rendering the cursor

## framebuffer update

The framebuffer update information is heavily inspired by the RFB protocol,
which can found here: https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#76encodings

Basically, some parts have been cut in order to allow for smaller packets
(which we want if we're sending over an unreliable connection). The RFB protocol
also uses TCP exclusively, and allows for reusing zlib streams etc, which
can't be used if we allow packets to be dropped.

---------| ------ | ------------
n. bytes | type   | description
---------| ------ | ------------
	  01 | uint8  | type  (02)
	  01 | uint8  | number of rects (1-255)
	  .. | rect   | see below for definition
---------| ------ | ------------

###	rect

All rects have the following header:

n. bytes | type   | description
---------| ------ | ------------
	  02 | uint16 | x-position of rect (network byte order)
	  02 | uint16 | y-position of rect (network byte order)
	  02 | uint16 | width of rect (network byte order)
	  02 | uint16 | height of rect (network byte order)
	  01 | uint8  | type of rect (see below)

Each rect can have ony of the following types:

 - 00 - raw
 - 01 - solid
 - 02 - packed palette with 2 colours 
 - 03 - packed palette with 3 colours
 - ...
 - 15 - packed palette with 15 colours
 - 16 - copy rect

#### 00 raw

n. bytes | type   | description
-------- | ------ | ------------
   w*h*3 | PIXEL  | RGB pixel data covering the complete rect

#### 01 solid

n. bytes | type   | description
---------| ------ | ------------
	  3  | PIXEL  | RGB data to fill the rect with

#### 02 - 15 palette

The number of colours in the palette corresponds to [type]

n. bytes     | type   | description
------------ | ------ | ------------
 colours * 3 | PIXEL  | RGB pixel data for each colour in the palette (type)
           m | uint8  | packed palette data

if only two colours are present in the palette, each pixel corresponds to one bit (0 or 1),
three colours give 2 bits, four colours give 2 bits, etc. up to 4 bits, which means that
the size of the packed palette data will be:

	2 colours: (width+7)/8 * height
	3-4 colours: (width+3)/4 * height
    5-15 colours: (width+1)/2 * height


#### 17 copy rect

n. bytes | type   | description
---------| ------ | ------------
	  2  | uint16 | x position to copy from (network byte order)
	  2  | uint16 | y-position to copy from (network byte order)
