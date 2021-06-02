# Video player thing for Casio fx calculators

Originally made to play bad apple but kinda grew out of it. So it's kind of a mess because it was just a test at first.

Inspired by the TI-84 bad apple [version](https://github.com/fb39ca4/badapple-ti84).

It doesn't have sound though. And the compression could be improved.

It technically handles any video as long as it's 128x64.

# Usage
The conversion script requires OpenCV, Numpy and bitstream.
```
sudo pip install bitstream
# OpenCV & Numpy can be installed through your
# distro's package mananger
```

The video file to convert must be 128x64, you can use ffmpeg to scale it down, like so
```
# OpenCV will read most file formats but i recommend something lossless.
ffmpeg -i <input> -map 0:v -vf scale=128x64 <output>
```

Then you can convert using the conv.py script for 1 bit depth or convdct.py for 8 bit depth but it requires the dct version of the player
```
python conv.py <input> [output]
(output defaults to data.bin)
```

For the player on the calculator, you can build it with make, it requires gint. There are 2 version of it the monochrome one and the dct one.
You need to select on of the two first with:
```
make switch_dct
# or
make switch_mono
```
Then to build for the calculator you simply do:
```
make
```
And send it to the calculator with:
```
# Uses p7
make send
```

Then send the data to the calculator aswell.
The data from the converter must be called "data.bin" on the calculator for it to work properly.

There's also a desktop version of the reader to test it. You can build with:
```
make reader
# or if you want to see the images (it requires SDL2)
make reader_sdl
```