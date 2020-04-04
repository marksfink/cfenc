#  cfenc - Cineform encoder/transcoder

Command-line Cineform encoder/transcoder for Linux and Mac.

```
usage: cfenc [options] -i <infile> <outfile>
-q, -quality <string>  Cineform encoding quality [fs1]
                        - low, medium, high, fs1, fs2, fs3
-rgb                   Encode RGB instead of YUV.  YUV is the default.
-c, -trc <int>         Force transfer characteristics [auto]
                        - 601 or 709
-t, -threads <int>     Number of threads to use for encoding [auto]
-l, -loglevel <string> Output verbosity [info]
                        - quiet, info, debug
-s, -video_size <WxH>  Video dimensions for raw input
-r, -framerate <N/D>   Frame rate for raw input in num/den format (like 30000/1001)
-p, -pix_fmt <string>  Pixel format for raw input.  Use FFmpeg values.
-a, -aspect <N:D>      Force display aspect ratio [auto]
-vo                    Mux only the new Cineform video stream into the output file.
-i <infile>            Input file or pipe:
<outfile>              Output Cineform file -- typically mov or avi format.
```

<br/>

**OVERVIEW**

The program uses FFmpeg libraries (avformat, avcodec, swscale) for file i/o, demuxing/muxing, and decoding/scaling, and it uses the Cineform SDK for encoding.  We're just bridging the two together.

It takes input from a file or from stdin -- anything that FFmpeg supports.  It encodes the video stream as Cineform, decoding and converting pixel formats as necessary (and only as necessary).  It *copies* other streams.  Then it outputs to any file format that FFmpeg supports (and that supports Cineform), typically Quicktime MOV or AVI formats.  I tested with MOV and AVI only, but the program won't stop you from trying other formats.  Worst case, it fails.

When muxing to Quicktime MOV, you will see this warning message that comes from FFmpeg:

`Using MS style video codec tag, the file may be unplayable!`

You can ignore this; the file will be playable.  This is because FFmpeg does not include CFHD in its list of supported codecs for MOV, probably because it does not support Cineform encoding.

If the target file format does not support the audio codec, it throws an error and fails.  This is because it *copies* all streams other than video.  This can happen when going from MOV to AVI or vice versa.  You can use FFmpeg to convert the audio and file format first, then send to cfenc.  Or you can use the 'vo' option and mux only the Cineform video stream.

It will convert YUV to RGB and vice versa if you ask it to (input is one and output is the other).  I am reasonably confident that I configured swscale to do this optimally, but that is open to review and suggestions from the community.  Otherwise, my guiding principle is to do as little to the source video as possible before sending to the Cineform encoder.

It does not perform any other scaling, filtering or conversion of any kind on the video.  It keeps the same dimensions and frame rate.  If you want to perform additional scaling/filtering/conversions on the video, then use FFmpeg or Vapoursynth (or whatever you like) to do it, then feed it into cfenc.  Cfenc's purpose in life is to encode Cineform and provide just enough convenience features beyond that.

One key use-case (for me anyhow) is to process video with Vapoursynth and send to cfenc for encoding, so vspipe works fine with cfenc.  However you also need to use the -s, -r, and -p options (see above).  Or you could use Yuv4Mpeg.  If you use -s, -r, and -p, then two things:
- the program assumes it is receiving raw video (so don't use those arguments unless the input is raw video -- you'll just get garbage output otherwise)
- you must use all three arguments (or the program tells you to use all three and stops)

Note that Vapoursynth's pixel formats don't always align to FFmpeg's pixel formats.  For instance, Vapoursynth's RGB48 is actually gbrp16le to FFmpeg (and thus cfenc) -- not rgb48le as you might expect.  Who's right, who's wrong on that?  I don't know.  It's confusing anyway.

THE GOOD

It uses the multithreaded Cineform encoder.  I've tested it with many formats and codecs and it works.

THE BAD

There is room for improvement (understatement) with threading and performance and my immediate plans are to work on that.  It's still better than running Windows software under wine.

The program does not support alpha channels, interlaced content, or Bayer pixel formats.  However, they could all be added without too much fuss.  I just personally don't need them.

THE UGLY

It is tested against and works with the FFmpeg 4.x API.  It does not work with FFmpeg 3.x.  I started down the road of supporting 3.x, but ran into issues that were taking too much of my time and I stopped.  Happy for someone to contribute that work if they care enough to.

Windows is not supported and won't be unless someone else contributes that work.  Cineform encoding on Windows is not really an issue anyway.  The glaring gap is on Linux.

<br/>

**BUILD INSTRUCTIONS**

You need FFmpeg 4.x installed with the usual av libraries (avformat, avcodec, etc) -- and compiled with support for formats and codecs that you want to work with cfenc.  You also need the include header files for the av libraries.  For example, on Ubuntu, you need to install avformat-dev, avcodec-dev, avutil-dev, and swscale-dev.  On a Mac, installing FFmpeg with brew installs everything you need.

You need to build and install the Cineform SDK from https://github.com/gopro/cineform-sdk.  Specifically you need libCFHDCodec.a and the include header files (and you'll get both by building/installing the SDK).  On Ubuntu, I needed to install uuid-dev as a prereq for the SDK (I don't think that is documented).

Then download this repo and in the repo dir...
```
mkdir build
cd build
cmake ..
make
sudo make install
```
Iow, a typical cmake build process.  It builds a single file (cfenc) and copies it into /usr/local/bin.

Happy encoding!
