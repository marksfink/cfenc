/*
 * Copyright (c) 2020 Mark Fink
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "version.h"
#include <unistd.h>
#include <getopt.h>
#include <regex>
#include <chrono>
#include <thread>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/pixdesc.h>
}

#include <cineformsdk/CFHDEncoder.h>
#include <cineformsdk/CFHDMetadata.h>
#include <cineformsdk/ver.h>

#ifdef __linux__
    #undef av_err2str
    #define av_err2str(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)
#endif

int g_width;
int g_height;


static void show_banner(void)
{
    av_log(nullptr, AV_LOG_INFO,
           "cfenc version %d.%d -- Cineform encoder/transcoder\n",
           cfenc_VERSION_MAJOR, cfenc_VERSION_MINOR);
    unsigned int version = avformat_version();
    av_log(nullptr, AV_LOG_INFO, "lib%-11s %2d.%3d.%3d\n", "avformat",
           AV_VERSION_MAJOR(version), AV_VERSION_MINOR(version), AV_VERSION_MICRO(version));
    version = avcodec_version();
    av_log(nullptr, AV_LOG_INFO, "lib%-11s %2d.%3d.%3d\n", "avcodec",
           AV_VERSION_MAJOR(version), AV_VERSION_MINOR(version), AV_VERSION_MICRO(version));
    version = avutil_version();
    av_log(nullptr, AV_LOG_INFO, "lib%-11s %2d.%3d.%3d\n", "avutil",
           AV_VERSION_MAJOR(version), AV_VERSION_MINOR(version), AV_VERSION_MICRO(version));
    version = swscale_version();
    av_log(nullptr, AV_LOG_INFO, "lib%-11s %2d.%3d.%3d\n", "swscale",
           AV_VERSION_MAJOR(version), AV_VERSION_MINOR(version), AV_VERSION_MICRO(version));
    av_log(nullptr, AV_LOG_INFO, "lib%-11s %2d.%3d.%3d\n\n", "CFHDCodec",
           kCFHDCodecVersionMajor, kCFHDCodecVersionMinor, kCFHDCodecVersionRevision);
}


static void show_usage(void)
{
    av_log(nullptr, AV_LOG_INFO,
    "usage: cfenc [options] -i <infile> <outfile>\n"
    "-q, -quality <string>  Cineform encoding quality [fs1]\n"
    "                            - low, medium, high, fs1, fs2, fs3\n"
    "-rgb                   Encode RGB instead of YUV.  YUV is the default.\n"
    "-c, -trc <int>         Force transfer characteristics [auto]\n"
    "                            - 601 or 709\n"
    "-t, -threads <int>     Number of threads to use for encoding [auto]\n"
    "-l, -loglevel <string> Output verbosity [info]\n"
    "                            - quiet, info, debug\n"
    "-s, -video_size <WxH>  Video dimensions for raw input\n"
    "-r, -framerate <N/D>   Frame rate for raw input in num/den format (like 30000/1001)\n"
    "-p, -pix_fmt <string>  Pixel format for raw input.  Use FFmpeg values.\n"
    "-a, -aspect <N:D>      Force display aspect ratio [auto]\n"
    "-vo                    Mux only the new Cineform video stream into the output file.\n"
    "-i <infile>            Input file or pipe:\n"
    "<outfile>              Output Cineform file -- typically mov or avi format.\n");
}


struct CliOptions
{
    // It's easier to use C strings with avformat functions.
    // using underscore naming convention to align with av naming
    const char *input;
    const char *output;
    std::string quality;
    bool b_rgb;
    int trc;
    int threads;
    const char *video_size;
    const char *framerate;
    AVRational r_frame_rate;
    AVRational aspect;
    const char *pix_fmt_name;
    bool b_video_only;

    CliOptions()
    {
        quality = "fs1";
        b_rgb = false;
        trc = 0;
        threads = 0;
        video_size = nullptr;
        framerate = nullptr;
        aspect.num = 0;
        aspect.den = 0;
        pix_fmt_name = nullptr;
        b_video_only = false;
    }

    void parse(int argc, char **argv);
};


void CliOptions::parse(int argc, char **argv)
{
    bool b_show_help = false;
    int c = 0;
    int raw_param = 0;
    int rgb = 0;
    int video_only = 0;

    while (1)
    {
        static struct option long_options[] =
        {
            {"quality",   required_argument, 0,          'q'},
            {"rgb",       no_argument,       &rgb,        1 },
            {"trc",       required_argument, 0,          'c'},
            {"threads",   required_argument, 0,          't'},
            {"loglevel",  required_argument, 0,          'l'},
            {"video_size",required_argument, 0,          's'},
            {"framerate", required_argument, 0,          'r'},
            {"pix_fmt",   required_argument, 0,          'p'},
            {"aspect",    required_argument, 0,          'a'},
            {"vo",        no_argument,       &video_only, 1 },
            {0, 0, 0, 0}
        };

        int option_index = 0;

        if (c == -1)
            break;

        c = getopt_long_only(argc, argv, "q:c:t:l:s:r:p:a:i:h",
                             long_options, &option_index);

        switch (c)
        {
            case 0:
                 break;
            case 'q':
                quality = optarg;
                if (quality != "low" && quality != "medium" && quality != "high" &&
                    quality != "fs1" && quality != "fs2"    && quality != "fs3")
                {
                    av_log(nullptr, AV_LOG_ERROR, "Invalid quality setting.\n");
                    b_show_help = true;
                }
                break;
            case 'c':
                trc = atoi(optarg);
                if (trc != 601 && trc != 709)
                {
                    av_log(nullptr, AV_LOG_ERROR, "Invalid trc setting.\n");
                    b_show_help = true;
                }
                break;
            case 't':
                threads = atoi(optarg);
                if (threads < 0)
                {
                    av_log(nullptr, AV_LOG_ERROR, "Threads must be >= 0.\n");
                    b_show_help = true;
                }
                break;
            case 'l':
            {
                std::string s_loglevel = optarg;
                if (s_loglevel == "quiet")
                    av_log_set_level(AV_LOG_QUIET);
                else if (s_loglevel == "info")
                    av_log_set_level(AV_LOG_INFO);
                else if (s_loglevel == "debug")
                    av_log_set_level(AV_LOG_DEBUG);
                else
                {
                    av_log(nullptr, AV_LOG_ERROR, "Invalid loglevel setting.\n");
                    b_show_help = true;
                }
                break;
            }
            case 's':
            {
                video_size = optarg;
                std::cmatch sm;
                if (std::regex_match(video_size, sm, std::regex("([0-9]+)x([0-9]+)")))
                {
                    g_width = stoi(sm[1].str());
                    g_height = stoi(sm[2].str());
                    if (g_width < 1 || g_height < 1)
                    {
                        av_log(nullptr, AV_LOG_ERROR, "Invalid video_size setting.\n");
                        b_show_help = true;
                    }
                }
                raw_param++;
                break;
            }
            case 'r':
            {
                framerate = optarg;
                std::cmatch sm;
                if (std::regex_match(framerate, sm, std::regex("([0-9]+)/([0-9]+)")))
                {
                    r_frame_rate.num = stoi(sm[1].str());
                    r_frame_rate.den = stoi(sm[2].str());
                    if (r_frame_rate.num < 1 || r_frame_rate.den < 1)
                    {
                        av_log(nullptr, AV_LOG_ERROR, "Invalid frame_rate setting.\n");
                        b_show_help = true;
                    }
                }
                raw_param++;
                break;
            }
            case 'p':
            {
                pix_fmt_name = optarg;
                AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
                if (pix_fmt == AV_PIX_FMT_NONE)
                {
                    av_log(nullptr, AV_LOG_ERROR, "Invalid pixel format.\n");
                    b_show_help = true;
                }
                raw_param++;
                break;
            }
            case 'a':
            {
                std::string s_aspect = optarg;
                std::smatch sm;
                if (std::regex_match(s_aspect, sm, std::regex("([0-9]+):([0-9]+)")))
                {
                    aspect.num = stoi(sm[1].str());
                    aspect.den = stoi(sm[2].str());
                    if (aspect.num < 1 || aspect.den < 1)
                    {
                        av_log(nullptr, AV_LOG_ERROR, "Invalid aspect ratio setting.\n");
                        b_show_help = true;
                    }
                }
                break;
            }
            case 'i':
                input = optarg;
                break;
            case 'h':
                b_show_help = true;
                break;
            case '?':
                b_show_help = true;
                break;
        }
    }

    if (rgb) b_rgb = true;
    if (video_only) b_video_only = true;

    if (optind == argc - 1)
        output = argv[optind];
    else
        b_show_help = true;

    if (raw_param != 0 && raw_param != 3)
    {
        av_log(nullptr, AV_LOG_ERROR,
        "If you specify video size, frame rate, and pixel format, we assume the input is raw\n"
        "video and you need to set all 3.  You set %d of them.\n", raw_param);
        b_show_help = true;
    }

    if (b_show_help || ! input)
    {
        show_usage();
        throw 1;
    }

    if (strcmp(input, output) == 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Input and output files are the same.\n");
        throw 1;
    }
}


struct CFHD_Encoder
{
    struct CFHD_AVData
    {
        uint8_t *data;
        int64_t pts;
        int64_t duration;
        
        CFHD_AVData(uint8_t *data, int pitch, int64_t pts, int64_t duration)
        {
            size_t size = (size_t)(pitch * g_height);
            this->data = (uint8_t*)malloc(size);
            memcpy(this->data, data, size);
            this->pts = pts;
            this->duration = duration;
        }
    };

    struct CFHD_Sample
    {
        CFHD_SampleBufferRef buffer;
        uint32_t frame_num;
        uint8_t *data;
        size_t size;
        int64_t pts;
        int64_t duration;

        CFHD_Sample()
        {
            buffer = nullptr;
            frame_num = 0;
            data = nullptr;
            size = 0;
            pts = 0;
            duration = 0;
        }
    };

    CFHD_EncoderPoolRef pool;
    CFHD_MetadataRef metadata;
    CFHD_Sample sample;
    CFHD_PixelFormat pix_fmt;
    CFHD_EncodedFormat enc_fmt;
    CFHD_EncodingFlags flags;
    CFHD_EncodingQuality quality;
    int threads;
    int queue_size;
    int queued;
    // queue is necessary to avoid a data race condition with the read/decode thread(s)
    std::vector<CFHD_AVData> queue;

    CFHD_Encoder(bool input_is_8_bit, int rgb, std::string quality, int trc, int threads)
    {
        this->quality = set_quality(quality);
        flags = CFHD_ENCODING_FLAGS_NONE;
        queued = 0;

        if (rgb)
        {
            // We scale everything to RGB48 because it simplifies things for us and because
            // Cineform does that anyway so we're not wasting cycles.
            pix_fmt = CFHD_PIXEL_FORMAT_RG48;
            enc_fmt = CFHD_ENCODED_FORMAT_RGB_444;
        }
        else
        {
            // Cineform encodes 8-bit YUV efficiently; scaling to 10-bit here would waste cycles.
            // We use v210 when our input video is already > 8-bit.
            if (input_is_8_bit)
                pix_fmt = CFHD_PIXEL_FORMAT_YUY2;
            else
                pix_fmt = CFHD_PIXEL_FORMAT_V210;
            enc_fmt = CFHD_ENCODED_FORMAT_YUV_422;
            if (trc == 0)
            {
                if (g_width <= 720)
                    flags |= CFHD_ENCODING_FLAGS_YUV_601;
            }
            else if (trc == 601)
                flags |= CFHD_ENCODING_FLAGS_YUV_601;
        }

        if (threads > 0) this->threads = threads;
        else
        {
            this->threads = std::thread::hardware_concurrency() - 1;
            if (this->threads <= 0) this->threads = 1;
        }
        queue_size = (int)round((float)this->threads * 1.5);
        av_log(nullptr, AV_LOG_INFO, "Encoding threads: %d\n", this->threads);
    }

    ~CFHD_Encoder()
    {
        av_log(nullptr, AV_LOG_DEBUG, "CFHD_Encoder destructor called.\n");
        for (CFHD_AVData i : queue)
        {
            free(i.data);
            i.data = nullptr;
        }
        if (sample.data)
        {
            CFHD_ReleaseSampleBuffer(pool, sample.buffer);
            sample.buffer = nullptr;
            sample.data = nullptr;
        }
        if (metadata)
        {
            CFHD_MetadataClose(metadata);
            metadata = nullptr;
        }
        if (pool)
        {
            CFHD_ReleaseEncoderPool(pool);
            pool = nullptr;
        }
    }

    bool start();
    bool push(uint8_t*, int, int, int64_t, int64_t);
    bool pop();

private:
    CFHD_EncodingQuality set_quality(std::string);
};


CFHD_EncodingQuality CFHD_Encoder::set_quality(std::string quality)
{
    const char *c_quality;
    CFHD_EncodingQuality cfhd_quality = CFHD_ENCODING_QUALITY_DEFAULT;

    if (quality == "low")
    {
        cfhd_quality = CFHD_ENCODING_QUALITY_LOW;
        c_quality = "Low";
    }
    else if (quality == "medium")
    {
        cfhd_quality = CFHD_ENCODING_QUALITY_MEDIUM;
        c_quality = "Medium";
    }
    else if (quality == "high")
    {
        cfhd_quality = CFHD_ENCODING_QUALITY_HIGH;
        c_quality = "High";
    }
    else if (quality == "fs1")
    {
        cfhd_quality = CFHD_ENCODING_QUALITY_FILMSCAN1;
        c_quality = "Film Scan 1";
    }
    else if (quality == "fs2")
    {
        cfhd_quality = CFHD_ENCODING_QUALITY_FILMSCAN2;
        c_quality = "Film Scan 2";
    }
    else if (quality == "fs3")
    {
        cfhd_quality = CFHD_ENCODING_QUALITY_FILMSCAN3;
        c_quality = "Film Scan 3";
    }
    av_log(nullptr, AV_LOG_INFO, "\nCineform quality: %s\n", c_quality);
    return cfhd_quality;
}


bool CFHD_Encoder::start()
{
    CFHD_Error err = CFHD_ERROR_OKAY;

    err = CFHD_MetadataOpen(&metadata);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: MetadataOpen failed with error code: %d\n", err);
        return false;
    }
    err = CFHD_CreateEncoderPool(&pool, threads, queue_size, nullptr);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: CreateEncoderPool failed with error code: %d\n", err);
        return false;
    }
    err = CFHD_AttachEncoderPoolMetadata(pool, metadata);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: AttachEncoderPoolMetadata failed with error code: %d\n", err);
        return false;
    }
    int videochannels = 1;  //1 - 2D, 2 - 3D;
    err = CFHD_MetadataAdd(metadata, TAG_VIDEO_CHANNELS, METADATATYPE_UINT32, 4,
                           (uint32_t *)&videochannels, false);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: MetadataAdd failed with error code: %d\n", err);
        return false;
    }
    err = CFHD_PrepareEncoderPool(pool, g_width, g_height, pix_fmt, enc_fmt, flags, quality);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: PrepareEncoderPool failed with error code: %d\n", err);
        return false;
    }
    err = CFHD_AttachEncoderPoolMetadata(pool, metadata);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: AttachEncoderPoolMetadata failed with error code: %d\n", err);
        return false;
    }
    err = CFHD_StartEncoderPool(pool);
    if (err)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "CFHD_Encoder::start: StartEncoderPool failed with error code: %d\n", err);
        return false;
    }
    return true;
}


bool CFHD_Encoder::push(uint8_t *data, int pitch, int frame_num, int64_t pts, int64_t duration)
{
    CFHD_Error err = CFHD_ERROR_OKAY;

    while (1)
    {
        if (queued < queue_size)
        {
            err = CFHD_MetadataAdd(metadata, TAG_UNIQUE_FRAMENUM, METADATATYPE_UINT32, 4,
                                   (uint32_t *)&(frame_num), false);
            if (err)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "CFHD_Encoder::push: MetadataAdd failed with error code: %d\n", err);
                return false;
            }

            int i = (frame_num - 1) % queue_size;
            if (i == queue.size())
                queue.push_back(CFHD_AVData(data, pitch, pts, duration));
            else
            {
                memcpy(queue[i].data, data, (size_t)(pitch * g_height));
                queue[i].pts = pts;
                queue[i].duration = duration;
            }

            err = CFHD_EncodeAsyncSample(pool, frame_num, queue[i].data, pitch, metadata);
            if (err)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "CFHD_Encoder::push: EncodeAsyncSample failed with error code: %d\n", err);
                return false;
            }
            queued++;
            return true;
        }
        else if (! pop())
            return false;
    }
}


bool CFHD_Encoder::pop()
{
    CFHD_Error err = CFHD_ERROR_OKAY;

    if (CFHD_ERROR_OKAY == CFHD_TestForSample(pool, &sample.frame_num, &sample.buffer))
    {
        err = CFHD_GetEncodedSample(sample.buffer, (void **)&sample.data, &sample.size);
        if (err)
        {
            av_log(nullptr, AV_LOG_ERROR,
                   "CFHD_Encoder::pop: GetEncodedSample failed with error code: %d\n", err);
            return false;
        }
        int i = (sample.frame_num - 1) % queue_size;
        sample.pts = queue[i].pts;
        sample.duration = queue[i].duration;
        queued--;
    }
    else usleep(10000);
    return true;
}


struct CFHD_Transcoder
{
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    AVCodecContext *dec_ctx;
    AVStream *input;
    CFHD_Encoder *cfhd;
    SwsContext *sws_ctx;
    AVCodecContext *v210_ctx;
    AVPacket *in_pkt;
    AVPacket *out_pkt;
    AVFrame *in_frame;
    AVFrame *out_frame;
    bool b_video_only;

    CFHD_Transcoder(bool b_video_only)
    {
        ifmt_ctx = avformat_alloc_context();
        ofmt_ctx = avformat_alloc_context();
        dec_ctx = avcodec_alloc_context3(nullptr);
        input = nullptr;
        cfhd = nullptr;
        sws_ctx = nullptr;
        v210_ctx = nullptr;
        in_pkt = av_packet_alloc();
        out_pkt = av_packet_alloc();
        in_frame = nullptr;
        out_frame = nullptr;
        this->b_video_only = b_video_only;

        if (! (ifmt_ctx && ofmt_ctx && dec_ctx && in_pkt && out_pkt))
        {
            av_log(nullptr, AV_LOG_ERROR, "CFHD_Transcoder: initialization failed\n");
            throw 4;
        }
    }

    ~CFHD_Transcoder()
    {
        av_log(nullptr, AV_LOG_DEBUG, "CFHD_Transcoder destructor called.\n");
        if (out_frame) av_frame_free(&out_frame);
        if (in_frame) av_frame_free(&in_frame);
        if (v210_ctx) avcodec_free_context(&v210_ctx);
        if (sws_ctx) sws_freeContext(sws_ctx);
        av_packet_free(&out_pkt);
        av_packet_free(&in_pkt);
        if (cfhd) delete cfhd;
        avcodec_free_context(&dec_ctx);
        if (ofmt_ctx->oformat && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
        ofmt_ctx = nullptr;
        avformat_close_input(&ifmt_ctx);
    }

    void open_input(CliOptions*);
    void open_output(CliOptions*);
    void process(CliOptions*);

private:
    void guess_channel_layout(AVStream*, int);
    bool encode();
    bool transcode();
    bool transcode_packet();
    bool init_scaler(AVPixelFormat, bool, int);
    bool init_v210_encoder();
    bool encode_v210();
    bool write_cfhd_sample();
};


void CFHD_Transcoder::open_input(CliOptions *cliopt)
{
    int ret;
    AVCodec *dec;

    // If video_size has a value, then...
    // 1 - we assume this is raw video.
    // 2 - pix_fmt_name and framerate must also be set per CliOptions validation
    if (cliopt->video_size)
    {
        AVInputFormat *fmt = av_find_input_format("rawvideo");
        AVDictionary *options = nullptr;
        av_dict_set(&options, "video_size", cliopt->video_size, 0);
        av_dict_set(&options, "pixel_format", cliopt->pix_fmt_name, 0);
        av_dict_set(&options, "framerate", cliopt->framerate, 0);
        if ((ret = avformat_open_input(&ifmt_ctx, cliopt->input, fmt, &options)) < 0)
        {
            av_log(nullptr, AV_LOG_ERROR,
                   "Failed to open '%s':\n%s\n", cliopt->input, av_err2str(ret));
            throw 2;
        }
        av_dict_free(&options);
    }
    else if ((ret = avformat_open_input(&ifmt_ctx, cliopt->input, nullptr, nullptr)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "Failed to open '%s':\n%s\n", cliopt->input, av_err2str(ret));
        throw 2;
    }

    if ((ret = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "Error finding video stream in '%s':\n%s\n", cliopt->input, av_err2str(ret));
        throw 2;
    }
    input = ifmt_ctx->streams[ret];
    // these may already be set, but they might not be, and this causes no harm if they are
    g_width = input->codecpar->width;
    g_height = input->codecpar->height;

    if (cliopt->framerate)
    {
        input->avg_frame_rate = cliopt->r_frame_rate;
        input->r_frame_rate = cliopt->r_frame_rate;
    }
    
    if (cliopt->aspect.num != 0)
        input->display_aspect_ratio = cliopt->aspect;
    else if (input->sample_aspect_ratio.num == 0)
    {
        input->sample_aspect_ratio.num = 1;
        input->sample_aspect_ratio.den = 1;
    }
    // this sets SAR from DAR (if DAR is set)
    avformat_find_stream_info(ifmt_ctx, nullptr);
    // this is needed...
    input->codecpar->sample_aspect_ratio = input->sample_aspect_ratio;

    if (input->nb_frames == 0 && ifmt_ctx->duration > 0)
    {
        av_log(nullptr, AV_LOG_INFO, "Estimating frame count from duration\n");
        float dur = (float)ifmt_ctx->duration / 1000000;
        float rate = (float)input->r_frame_rate.num / (float)input->r_frame_rate.den;
        input->nb_frames = (int64_t)(dur * rate);
    }

    av_dump_format(ifmt_ctx, 0, cliopt->input, 0);

    // Check if the video is already in a format that we can send direct to the cfhd encoder.
    if (input->codecpar->codec_id == AV_CODEC_ID_RAWVIDEO)
    {
        if (cliopt->b_rgb)
        {
            if ((AVPixelFormat)input->codecpar->format == AV_PIX_FMT_RGB48LE)
                return;
        }
        else if ((AVPixelFormat)input->codecpar->format == AV_PIX_FMT_YUYV422)
            return;
    }

    // Otherwise we need to decode/scale it.
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "open_input: avcodec_alloc_context3 failed\n");
        throw 2;
    }
    if ((ret = avcodec_parameters_to_context(dec_ctx, input->codecpar)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "open_input: avcodec_parameters_to_context failed:\n%s\n", av_err2str(ret));
        throw 2;
    }
    if ((ret = avcodec_open2(dec_ctx, dec, nullptr)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "open_input: avcodec_open2 failed:\n%s\n", av_err2str(ret));
        throw 2;
    }
}


void CFHD_Transcoder::guess_channel_layout(AVStream *ist, int i)
{
    char layout_name[256];

    ist->codecpar->channel_layout = av_get_default_channel_layout(ist->codecpar->channels);
    av_get_channel_layout_string(layout_name, sizeof(layout_name),
                                 ist->codecpar->channels, ist->codecpar->channel_layout);
    av_log(NULL, AV_LOG_WARNING, "Guessed channel layout for stream #0:%u: %s\n", i, layout_name);
}


void CFHD_Transcoder::open_output(CliOptions *cliopt)
{
    int i, ret;
    AVStream *ist, *ost;
    AVDictionaryEntry *tag = nullptr;
    
    if ((ret = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, cliopt->output)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "open_output: avformat_alloc_output_context2 failed:\n%s\n", av_err2str(ret));
        throw 3;
    }
    ofmt_ctx->oformat->video_codec = AV_CODEC_ID_CFHD;
    ofmt_ctx->oformat->audio_codec = AV_CODEC_ID_NONE;

    // copy container metadata
    while ((tag = av_dict_get(ifmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        // ignore these tags
        if (strcmp(tag->key, "major_brand") != 0 && strcmp(tag->key, "minor_version") != 0 &&
            strcmp(tag->key, "compatible_brands") !=0 && strcmp(tag->key, "encoder") != 0)
        {
            if ((ret = av_dict_set(&ofmt_ctx->metadata, tag->key, tag->value, 0)) < 0)
                av_log(nullptr,
                       AV_LOG_WARNING, "Error copying container metadata:\n%s\n", av_err2str(ret));
        }

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        ist = ifmt_ctx->streams[i];
        
        if (cliopt->b_video_only && ist != input)
            continue;

        if (! (ost = avformat_new_stream(ofmt_ctx, nullptr)))
        {
            av_log(nullptr, AV_LOG_ERROR,
                   "open_output: avformat_new_stream failed for for stream #0:%u\n", i);
            throw 3;
        }

        // Copy track language
        if ((tag = av_dict_get(ist->metadata, "language", nullptr, 0)))
            if ((ret = av_dict_set(&ost->metadata, tag->key, tag->value, 0)) < 0)
                av_log(nullptr, AV_LOG_WARNING, "Error copying language for stream #0:%u:\n%s\n",
                                                i, av_err2str(ret));

        if (ist == input)
        {
            ost->codecpar->codec_id = AV_CODEC_ID_CFHD;
            ost->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            if (cliopt->b_rgb) ost->codecpar->format = AV_PIX_FMT_GBRP12LE;
            else ost->codecpar->format = AV_PIX_FMT_YUV422P10LE;
            ost->codecpar->width = g_width;
            ost->codecpar->height = g_height;
            ost->codecpar->video_delay = ist->codecpar->video_delay;
            ost->time_base = av_inv_q(ist->r_frame_rate);
            ost->r_frame_rate = ist->r_frame_rate;
            ost->avg_frame_rate = ist->avg_frame_rate;
            // this is required for mov files
            ost->codecpar->sample_aspect_ratio = ist->sample_aspect_ratio;
            // this is required for avi files
            ost->sample_aspect_ratio = ist->sample_aspect_ratio;
        }
        else
        {
            ost->time_base = ist->time_base;
            if ((avcodec_parameters_copy(ost->codecpar, ist->codecpar)) < 0)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "open_output: avcodec_parameters_copy failed for stream #0:%u\n", i);
                throw 3;
            }
            if (ist->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                ofmt_ctx->oformat->audio_codec = ist->codecpar->codec_id;
                if (! ost->codecpar->channel_layout)
                    guess_channel_layout(ost, i);
            }
            if (ist->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
                ofmt_ctx->oformat->subtitle_codec = ist->codecpar->codec_id;
        }
    }

    if ((ret = avio_open(&ofmt_ctx->pb, cliopt->output, AVIO_FLAG_WRITE)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open_output: avio_open failed:\n%s\n", av_err2str(ret));
        throw 3;
    }
    if ((ret = avformat_write_header(ofmt_ctx, nullptr)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "open_output: avformat_write_header failed:\n%s\n",
               av_err2str(ret));
        throw 3;
    }

    av_dump_format(ofmt_ctx, 0, cliopt->output, 1);
}


bool CFHD_Transcoder::write_cfhd_sample()
{
    int ret;

    if (cfhd->sample.size > 0)
    {
        out_pkt->data = cfhd->sample.data;
        out_pkt->size = (int)cfhd->sample.size;
        out_pkt->flags |= AV_PKT_FLAG_KEY;
        out_pkt->duration = cfhd->sample.duration;
        out_pkt->pts = out_pkt->dts = cfhd->sample.pts;

        if (b_video_only)
            out_pkt->stream_index = 0;
        else
            out_pkt->stream_index = input->index;
        AVStream *output = ofmt_ctx->streams[out_pkt->stream_index];
        av_packet_rescale_ts(out_pkt, input->time_base, output->time_base);

        if ((ret = av_write_frame(ofmt_ctx, out_pkt)) < 0)
        {
            av_log(nullptr, AV_LOG_ERROR,
                   "write_cfhd_sample: av_write_frame failed:\n%s\n", av_err2str(ret));
            return false;
        }
        if (input->nb_frames > 0)
            av_log(nullptr, AV_LOG_INFO,
                   "           Frame: %u / %lld\r", cfhd->sample.frame_num, input->nb_frames);
        else
            av_log(nullptr, AV_LOG_INFO,
                   "           Frame: %u\r", cfhd->sample.frame_num);

        cfhd->sample.data = nullptr;
        cfhd->sample.size = 0;
        CFHD_ReleaseSampleBuffer(cfhd->pool, cfhd->sample.buffer);
    }
    return true;
}


bool CFHD_Transcoder::init_v210_encoder()
{
    int ret;

    const AVCodec *v210 = avcodec_find_encoder_by_name("v210");
    if (! v210)
    {
        av_log(nullptr, AV_LOG_ERROR, "v210 codec not found with libavcodec.\n");
        av_log(nullptr, AV_LOG_ERROR, "Check the version and build options for libavcodec.\n");
        return false;
    }
    if (! (v210_ctx = avcodec_alloc_context3(v210)))
    {
        av_log(nullptr, AV_LOG_ERROR, "init_v210_encoder: avcodec_alloc_context3 failed\n");
        return false;
    }

    v210_ctx->width = g_width;
    v210_ctx->height = g_height;
    v210_ctx->time_base = input->time_base;
    v210_ctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;

    if ((ret = avcodec_open2(v210_ctx, v210, nullptr)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "init_v210_encoder: avcodec_open2 failed:\n%s\n", av_err2str(ret));
        return false;
    }
    return true;
}


bool CFHD_Transcoder::encode_v210()
{
    int ret;

    if ((ret = avcodec_send_frame(v210_ctx, out_frame)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "encode_v210: avcodec_send_frame failed:\n%s\n", av_err2str(ret));
        return false;
    }
    while (1)
    {
        ret = avcodec_receive_packet(v210_ctx, out_pkt);
        if (ret == 0)
        {
            int pitch = out_pkt->buf->size / g_height;
            if (! cfhd->push(out_pkt->buf->data, pitch, dec_ctx->frame_number,
                             in_frame->pts, in_frame->pkt_duration))
                return false;
            av_packet_unref(out_pkt);
            return true;
        }
        else if (ret != AVERROR(EAGAIN))
        {
            av_log(nullptr, AV_LOG_ERROR,
                   "encode_v210: avcodec_receive_packet failed:\n%s\n", av_err2str(ret));
            return false;
        }
    }
}


bool CFHD_Transcoder::init_scaler(AVPixelFormat new_pix_fmt, bool accurate, int trc)
{
    AVColorSpace colorspace;
    const AVPixelFormat src_pix_fmt = (AVPixelFormat)input->codecpar->format;
    int flags = SWS_BICUBIC;
    const int *table;

    if (accurate)
        flags |= SWS_ACCURATE_RND | SWS_FULL_CHR_H_INT;

    sws_ctx = sws_getContext(g_width, g_height, src_pix_fmt,
                             g_width, g_height, new_pix_fmt,
                             flags, nullptr, nullptr, nullptr);
    if (! sws_ctx)
    {
        av_log(nullptr, AV_LOG_ERROR, "init_scaler: sws_getContext failed\n");
        return false;
    }

    switch(trc)
    {
        case 601:
            colorspace = AVCOL_SPC_BT470BG;
            break;
        case 709:
            colorspace = AVCOL_SPC_BT709;
            break;
        default:
            if (g_width <= 720)
                colorspace = AVCOL_SPC_BT470BG;
            else
                colorspace = AVCOL_SPC_BT709;
    }
    table = sws_getCoefficients(colorspace);
    sws_setColorspaceDetails(sws_ctx, table, 0, table, 0, 0, 65535, 65535);
    return true;
}


bool CFHD_Transcoder::transcode_packet()
{
    int ret;

    if ((ret = avcodec_send_packet(dec_ctx, in_pkt)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "transcode_packet: avcodec_send_packet failed:\n%s\n", av_err2str(ret));
        return false;
    }
    while (1)
    {
        ret = avcodec_receive_frame(dec_ctx, in_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return true;
        else if (ret < 0)
        {
            av_log(nullptr, AV_LOG_ERROR,
                   "transcode_packet: avcodec_receive_frame failed:\n%s\n", av_err2str(ret));
            return false;
        }

        if (sws_ctx)
        {
            if ((ret = sws_scale(sws_ctx, in_frame->data, in_frame->linesize, 0, g_height,
                                 out_frame->data, out_frame->linesize)) <= 0)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "transcode_packet: sws_scale failed:\n%s\n", av_err2str(ret));
                return false;
            }
        }
        else av_frame_ref(out_frame, in_frame);

        if (v210_ctx)
        {
            if (! encode_v210())
                return false;
        }
        else if (! cfhd->push(out_frame->data[0], out_frame->linesize[0], dec_ctx->frame_number,
                              in_frame->pts, in_frame->pkt_duration))
                return false;

        if (! write_cfhd_sample())
            return false;

        if (! sws_ctx) av_frame_unref(out_frame);
        av_frame_unref(in_frame);
    }
}


// Video data requires decoding/scaling to feed the CFHD encoder.
bool CFHD_Transcoder::transcode()
{
    int ret;

    av_log(nullptr, AV_LOG_DEBUG, "Decoding/scaling video then sending to the cfhd encoder.\n");
    while (1)
    {
        if (av_read_frame(ifmt_ctx, in_pkt) < 0)
            break;
        // if this is the video stream...
        if (ifmt_ctx->streams[in_pkt->stream_index] == input)
        {
            if (! transcode_packet())
               return false;
        }
        // else remux other streams
        else if (! b_video_only)
            if ((ret = av_write_frame(ofmt_ctx, in_pkt)) < 0)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "transcode: av_write_frame failed for stream #0:%u:\n%s\n",
                       in_pkt->stream_index, av_err2str(ret));
                return false;
            }
        av_packet_unref(in_pkt);
    }
    // flush the decoder
    av_packet_free(&in_pkt);
    if (! transcode_packet())
        return false;
    return true;
}


// Video data is already in a format we can feed to the CFHD encoder without decoding/scaling.
bool CFHD_Transcoder::encode()
{
    int ret;
    int frame_num = 0;

    av_log(nullptr, AV_LOG_DEBUG, "Sending video direct to the cfhd encoder.\n");
    while (1)
    {
        if (av_read_frame(ifmt_ctx, in_pkt) < 0)
            break;
        // if this is the video stream...
        if (ifmt_ctx->streams[in_pkt->stream_index] == input)
        {
            frame_num++;
            int pitch = in_pkt->buf->size / g_height;
            if (! (cfhd->push(in_pkt->buf->data, pitch, frame_num, in_pkt->pts, in_pkt->duration) &&
                   write_cfhd_sample()))
                return false;
        }
        // remux other streams
        else if (! b_video_only)
            if ((ret = av_write_frame(ofmt_ctx, in_pkt)) < 0)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "encode: av_write_frame failed for stream #0:%u:\n%s\n",
                       in_pkt->stream_index, av_err2str(ret));
                return false;
            }
        av_packet_unref(in_pkt);
    }
    return true;
}


void CFHD_Transcoder::process(CliOptions *cliopt)
{
    int ret;
    const AVPixFmtDescriptor *input_desc;
    bool input_is_8_bit = false;
    bool input_is_rgb = false;

    input_desc = av_pix_fmt_desc_get((AVPixelFormat)input->codecpar->format);
    if (input_desc->comp[0].depth == 8)
        input_is_8_bit = true;
    if (input_desc->flags & AV_PIX_FMT_FLAG_RGB)
        input_is_rgb = true;
    
    cfhd = new CFHD_Encoder(input_is_8_bit, cliopt->b_rgb, cliopt->quality,
                            cliopt->trc, cliopt->threads);
    if (! cfhd->start())
        throw 4;

    // codec_id will be set to a codec if we need to decode; otherwise, it is set to NONE.
    if (dec_ctx->codec_id == AV_CODEC_ID_NONE)
    {
        if (! encode())
            throw 4;
    }
    else
    {
        AVPixelFormat new_pix_fmt = AV_PIX_FMT_NONE;
        in_frame = av_frame_alloc();
        out_frame = av_frame_alloc();
        if (!(in_frame && out_frame))
        {
            av_log(nullptr, AV_LOG_ERROR, "process: frame allocation failed\n");
            throw 4;
        }

        if (cliopt->b_rgb)
            new_pix_fmt = AV_PIX_FMT_RGB48LE;
        else if (input_is_8_bit)
            new_pix_fmt = AV_PIX_FMT_YUYV422;
        else
        {
            if ((AVPixelFormat)input->codecpar->format != AV_PIX_FMT_YUV422P10LE)
                new_pix_fmt = AV_PIX_FMT_YUV422P10LE;
            if (! init_v210_encoder())
                throw 4;
        }

        if (new_pix_fmt != AV_PIX_FMT_NONE)
        {
            out_frame->width = g_width;
            out_frame->height = g_height;
            out_frame->format = new_pix_fmt;
            if ((ret = av_frame_get_buffer(out_frame, 0)) < 0)
            {
                av_log(nullptr, AV_LOG_ERROR,
                       "process: av_frame_get_buffer failed:\n%s\n", av_err2str(ret));
                throw 4;
            }
            // test if we are converting YUV->RGB or RGB->YUV, set scaler flags accordingly
            bool accurate = false;
            if (input_is_rgb != cliopt->b_rgb)
                accurate = true;
            if (! init_scaler(new_pix_fmt, accurate, cliopt->trc))
                throw 4;
        }

        if (! transcode())
            throw 4;
    }

    // flush the encoder
    while (cfhd->queued)
        if (! (cfhd->pop() && write_cfhd_sample()))
            throw 4;

    av_log(nullptr, AV_LOG_INFO, "\n");
    if ((ret = av_write_trailer(ofmt_ctx)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR,
               "process: av_write_trailer failed:\n%s\n", av_err2str(ret));
        throw 4;
    }
}


int main(int argc, char **argv)
{
    try
    {
        CliOptions cliopt;
        cliopt.parse(argc, argv);

        show_banner();

        CFHD_Transcoder tc(cliopt.b_video_only);
        tc.open_input(&cliopt);
        tc.open_output(&cliopt);

        auto start = std::chrono::high_resolution_clock::now();
        tc.process(&cliopt);
        auto stop = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        float seconds = (float)duration.count() / 1000;
        float fps = (float)tc.cfhd->sample.frame_num / seconds;

        av_log(nullptr, AV_LOG_INFO, "Encoded %d frames in %1.2f seconds (%1.2f fps)\n",
               tc.cfhd->sample.frame_num, seconds, fps);
    }
    catch (int e)
    {
        exit(e);
    }
}
