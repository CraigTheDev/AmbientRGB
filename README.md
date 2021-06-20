# AmbientRGB
Uses peripheral RGB LEDs to create ambient lighting by picking a colour from the screen in real time.

Currently very bare-bones and only supports Corsair devices via iCUE. Feel free to use, modify, or add support for more devices.

## In detail
Captures the main monitor in real time using windows GDI, capture size is reduced to 400x400 regardless of native resolution and runs at 5fps by default. This seems to be a good balance between responsiveness and keeping resource usage to a minimum, but can easily be changed. A colour histogram is then generated from the captured image by splitting up the RGB colour space into a 4x4x4 grid and counting up how many colours are in the image that fall into each of these grid cells. From this the 4 cells with the highest counts are used to create 4 colours that best represent the original captured image. 1 colour is then selected by scoring each of the 4 created colours mostly based on saturation, luminance, and count. The goal of this is to select the brightest and most vivid colour that best represents the captured image, avoiding darks and greys that will be a lot less visible when displayed on RGB lighting. Once the final colour has been selected it is then passed to any supported lighting APIs to be displayed as ambient lighting.

Setting UseDebugDisplay to TRUE will display a window that shows the captured image, the 4 most common colours found in the histogram, and the final selected colour that any RGB lighting will be set to.

## Supported Devices
Corsair iCUE (Should support all corsair devices that are controlled with the iCUE software).

## Authors
Craig Richardson - Initial work - twitter.com/CraigTheDev

## License
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this software dedicate any and all copyright interest in the software to the public domain. We make this dedication for the benefit of the public at large and to the detriment of our heirs and successors. We intend this dedication to be an overt act of relinquishment in perpetuity of all present and future rights to this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
