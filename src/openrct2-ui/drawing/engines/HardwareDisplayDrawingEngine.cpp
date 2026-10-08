/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "DrawingEngineFactory.hpp"

#include <SDL_hints.h>
#include <SDL_render.h>
#include <SDL_version.h>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <libyuv.h>
#include <libyuv/convert_from_argb.h>
#include <memory>
#include <mutex>
#include <openrct2/Diagnostic.h>
#include <openrct2/Game.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/Guard.hpp>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/drawing/LightFX.h>
#include <openrct2/drawing/X8DrawingEngine.h>
#include <openrct2/interface/Window.h>
#include <openrct2/paint/Paint.h>
#include <openrct2/scenes/title/TitleScene.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/scenes/title/TitleSequencePlayer.h>
#include <openrct2/scenes/title/TitleSequenceRender.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/ui/WindowManager.h>
#include <thread>
#ifdef ENABLE_VIDEO_RECORDING
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/log.h>
    #include <libavutil/opt.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
}
    #include <ctime>
#endif
#ifdef _WIN32
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::Drawing;
using namespace OpenRCT2::Ui;

bool gShouldRender = true;

#ifdef ENABLE_VIDEO_RECORDING
static FILE* gFFmpegLogFile = nullptr;
static std::mutex gFFmpegLogMutex;

static void CustomAVLogCallback(void* ptr, int level, const char* fmt, va_list vl)
{
    va_list vl2;
    va_copy(vl2, vl);

    av_log_default_callback(ptr, level, fmt, vl);

    std::lock_guard<std::mutex> lock(gFFmpegLogMutex);
    if (gFFmpegLogFile != nullptr)
    {
        char line[4096];
        int print_prefix = 1;
        av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
        fputs(line, gFFmpegLogFile);
        fflush(gFFmpegLogFile);
    }
    va_end(vl2);
}

static void UnregisterFFmpegLogging()
{
    av_log_set_callback(av_log_default_callback);
    std::lock_guard<std::mutex> lock(gFFmpegLogMutex);
    if (gFFmpegLogFile != nullptr)
    {
        fclose(gFFmpegLogFile);
        gFFmpegLogFile = nullptr;
    }
}

static void RegisterFFmpegLogging(const std::string& videoName)
{
    UnregisterFFmpegLogging();

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char timestampBuf[32];
    std::strftime(timestampBuf, sizeof(timestampBuf), "%Y%m%d-%H%M%S", &tm);

    std::string logFilename = "ffmpeg-" + videoName + "-" + std::string(timestampBuf) + ".log";

    std::lock_guard<std::mutex> lock(gFFmpegLogMutex);
    gFFmpegLogFile = fopen(logFilename.c_str(), "w");
    if (gFFmpegLogFile != nullptr)
    {
        LOG_INFO("FFmpeg log file created: %s", logFilename.c_str());
    }
    else
    {
        LOG_ERROR("Failed to create FFmpeg log file: %s", logFilename.c_str());
    }
    av_log_set_callback(CustomAVLogCallback);
}

static bool IsEncoderHW(const AVCodec* encoder)
{
    if (encoder->capabilities & AV_CODEC_CAP_HARDWARE)
        return true;

    for (int i = 0;; i++)
    {
        const AVCodecHWConfig* config = avcodec_get_hw_config(encoder, i);
        if (!config)
            break;
        if (config->methods & (AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX | AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX))
        {
            return true;
        }
    }

    static const char* hw_prefixes[] = { "nvenc", "vaapi", "qsv", "videotoolbox", "omx", "d3d11va", "dxva2", "amf" };
    for (const char* prefix : hw_prefixes)
    {
        if (strstr(encoder->name, prefix))
            return true;
    }

    return false;
}

static void LogHWAlternatives(AVCodecID codecId)
{
    const AVCodec* encoder = nullptr;
    void* i = nullptr;
    std::vector<std::string> alternatives;
    while ((encoder = av_codec_iterate(&i)))
    {
        if (av_codec_is_encoder(encoder) && encoder->id == codecId && IsEncoderHW(encoder))
        {
            alternatives.push_back(encoder->name);
        }
    }

    if (!alternatives.empty())
    {
        std::string list;
        for (const auto& name : alternatives)
        {
            if (!list.empty())
                list += ", ";
            list += name;
        }
        LOG_INFO("Hardware accelerated alternatives for this codec: %s", list.c_str());
    }
}

static int encode_frame(AVCodecContext* enc_ctx, AVFrame* frame, AVFormatContext* fmt_ctx, AVStream* st)
{
    int ret;

    // send the frame to the encoder
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0)
    {
        LOG_ERROR("Error sending a frame for encoding\n");
        return ret;
    }

    while (ret >= 0)
    {
        AVPacket* pkt = av_packet_alloc();
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_free(&pkt);
            return 0;
        }
        else if (ret < 0)
        {
            LOG_ERROR("Error during encoding\n");
            av_packet_free(&pkt);
            return ret;
        }

        av_packet_rescale_ts(pkt, enc_ctx->time_base, st->time_base);
        pkt->stream_index = st->index;

        const int keyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        ret = av_interleaved_write_frame(fmt_ctx, pkt);

        char type = '.';
        if (keyframe)
        {
            type = 'I';
        }
        else if (pkt->pts != pkt->dts && pkt->dts != AV_NOPTS_VALUE)
        {
            type = 'B';
        }

        av_packet_free(&pkt);
        if (ret < 0)
        {
            LOG_ERROR("Error while writing video frame\n");
            return ret;
        }

        printf("%c", type);
        fflush(stdout);
    }

    return 0;
}
#endif

struct EncodeThreadData
{
    std::mutex* SurfaceMutex;
    std::condition_variable* NotifyCV;
    std::atomic<int>* Ready;

#ifdef ENABLE_VIDEO_RECORDING
    AVFormatContext* formatContext{};
    AVCodecContext* codecContext{};
    AVStream* videoStream{};
    AVFrame* frame{};
    AVCodecContext* audioCodecContext{};
    AVStream* audioStream{};
    AVFrame* audioFrame{};
    SwrContext* swrContext{};
    int64_t nextAudioPts{ 0 };
    int64_t audioSamplesWritten{ 0 };
    std::atomic<bool> forceKeyframe{ false };
    int frameCount{ 0 };
#endif
    uint32_t width{};
    uint32_t height{};
    uint32_t scale{};
    bool yuv444{ false };

    // Double buffering: A/B pixel buffers
    std::unique_ptr<uint8_t[]> pixelBufferA{};
    std::unique_ptr<uint8_t[]> pixelBufferB{};
    size_t bufferSize{};

    // Which buffer is currently being used by the encoding thread
    std::atomic<int> activeBuffer{ 0 }; // 0 = A, 1 = B

    // Double buffering for audio: A/B audio buffers
    std::vector<uint8_t> audioBufferA{};
    std::vector<uint8_t> audioBufferB{};
};

static void EncodeThreadFunc(EncodeThreadData& etd)
{
    while (true)
    {
        std::unique_lock lock(*etd.SurfaceMutex);
        (*etd.NotifyCV).wait(lock, [&etd] { return *etd.Ready != 0; });
        if (*etd.Ready == 2)
        {
            printf("closing down thread");
            fflush(stdout);
            break;
        }
        *etd.Ready = 0;

        // Get the current buffer to encode
        uint8_t* currentPixelBuffer = (etd.activeBuffer == 0) ? etd.pixelBufferA.get() : etd.pixelBufferB.get();

        if (currentPixelBuffer && etd.bufferSize > 0)
        {
            auto scaledWidth = etd.width * etd.scale;
            auto scaledHeight = etd.height * etd.scale;

            // Create source surface from the pixel buffer
            SDL_Surface* tempSurface = SDL_CreateRGBSurfaceWithFormatFrom(
                currentPixelBuffer, etd.width, etd.height, 32, etd.width * 4, SDL_PIXELFORMAT_ARGB8888);

            if (tempSurface != nullptr)
            {
                // Create scaled surface buffer
                SDL_Surface* scaledSurface = SDL_CreateRGBSurfaceWithFormat(
                    0, scaledWidth, scaledHeight, 32, SDL_PIXELFORMAT_ARGB8888);

                if (scaledSurface != nullptr)
                {
                    // Scale the surface
                    if (SDL_BlitScaled(tempSurface, nullptr, scaledSurface, nullptr) == 0)
                    {
                        auto function = libyuv::ARGBToI420;
                        if (etd.yuv444)
                        {
                            function = libyuv::ARGBToI444;
                        }
                        // Convert to YUV for encoding
                        function(
                            static_cast<uint8_t*>(scaledSurface->pixels), scaledSurface->pitch,
#ifdef ENABLE_VIDEO_RECORDING
                            etd.frame->data[0], etd.frame->linesize[0], etd.frame->data[1], etd.frame->linesize[1],
                            etd.frame->data[2], etd.frame->linesize[2],
#else
                            nullptr, 0, nullptr, 0, nullptr, 0,
#endif
                            scaledWidth, scaledHeight);

#ifdef ENABLE_VIDEO_RECORDING
                        etd.frame->pts = etd.frameCount++;
                        if (etd.forceKeyframe.exchange(false))
                        {
                            etd.frame->pict_type = AV_PICTURE_TYPE_I;
                        }
                        else
                        {
                            etd.frame->pict_type = AV_PICTURE_TYPE_NONE;
                        }
                        encode_frame(etd.codecContext, etd.frame, etd.formatContext, etd.videoStream);
                        if (etd.frameCount % 100 == 0)
                        {
                            printf("%d", etd.frameCount);
                            fflush(stdout);
                        }

                        // Get current active audio buffer for encoding thread
                        const auto& activeAudioBuf = (etd.activeBuffer == 0) ? etd.audioBufferA : etd.audioBufferB;

                        // Encode audio for this frame if audio is initialized
                        if (etd.audioCodecContext && etd.swrContext && !activeAudioBuf.empty())
                        {
                            const uint8_t* inData[1] = { activeAudioBuf.data() };
                            int inSamples = static_cast<int>(activeAudioBuf.size() / 4); // 4 bytes per stereo sample (2x int16_t)

                            int frameSize = etd.audioCodecContext->frame_size;
                            if (frameSize <= 0)
                            {
                                frameSize = inSamples;
                            }

                            while (inSamples > 0 || swr_get_delay(etd.swrContext, 22050) >= frameSize)
                            {
                                int dstNbSamples = av_rescale_rnd(
                                    swr_get_delay(etd.swrContext, 22050) + inSamples, etd.audioCodecContext->sample_rate,
                                    22050, AV_ROUND_UP);

                                if (dstNbSamples < frameSize)
                                {
                                    // Feed remaining input to resampler buffer
                                    if (inSamples > 0)
                                    {
                                        swr_convert(etd.swrContext, nullptr, 0, inData, inSamples);
                                        inSamples = 0;
                                    }
                                    break;
                                }

                                av_frame_make_writable(etd.audioFrame);
                                etd.audioFrame->nb_samples = frameSize;

                                uint8_t** outData = etd.audioFrame->data;
                                int converted = swr_convert(
                                    etd.swrContext, outData, frameSize,
                                    inSamples > 0 ? inData : nullptr, inSamples);

                                inSamples = 0; // All input consumed by swr_convert

                                if (converted > 0)
                                {
                                    etd.audioFrame->nb_samples = converted;
                                    etd.audioFrame->pts = etd.nextAudioPts;
                                    etd.nextAudioPts += converted;

                                    encode_frame(etd.audioCodecContext, etd.audioFrame, etd.formatContext, etd.audioStream);
                                }
                            }
                        }
#endif
                    }
                    else
                    {
                        LOG_ERROR("SDL_BlitScaled failed: %s", SDL_GetError());
                    }
                    SDL_FreeSurface(scaledSurface);
                }
                SDL_FreeSurface(tempSurface);
            }
        }
    }
}

class HardwareDisplayDrawingEngine final : public X8DrawingEngine
{
private:
    constexpr static uint32_t kDirtyVisualTime = 40;
    constexpr static uint32_t kDirtyRegionAlpha = 100;

    IUiContext& _uiContext;
    SDL_Window* _window = nullptr;
    SDL_Renderer* _sdlRenderer = nullptr;
    SDL_Texture* _screenTexture = nullptr;
    SDL_Texture* _scaledScreenTexture = nullptr;
    SDL_PixelFormat* _screenTextureFormat = nullptr;
    uint32_t _paletteHWMapped[256] = { 0 };
    uint32_t _lightPaletteHWMapped[256] = { 0 };
    bool _yuv444 = false;

    bool _useVsync = true;

    std::vector<uint32_t> _dirtyVisualsTime;

    bool smoothNN = false;

#ifdef ENABLE_VIDEO_RECORDING
    AVFormatContext* _formatContext = nullptr;
    AVCodecContext* _codecContext = nullptr;
    AVStream* _videoStream = nullptr;
    AVFrame* _frame = nullptr;

    AVCodecContext* _audioCodecContext = nullptr;
    AVStream* _audioStream = nullptr;
    AVFrame* _audioFrame = nullptr;
    SwrContext* _swrContext = nullptr;
    double _audioSampleAcc = 0.0;
#endif

    std::thread EncodeThread{};
    std::mutex SurfaceMutex{};
    std::condition_variable NotifyCV{};
    std::atomic<int> Ready{};
    EncodeThreadData etd{};
    bool _videoInitialized = false;

public:
    explicit HardwareDisplayDrawingEngine(IUiContext& uiContext)
        : X8DrawingEngine(uiContext)
        , _uiContext(uiContext)
    {
        _window = static_cast<SDL_Window*>(_uiContext.GetWindow());

        const char* yuv444Env = getenv("YUV444");
        if (yuv444Env != nullptr && strlen(yuv444Env) > 0)
        {
            _yuv444 = true;
            LOG_INFO("Using YUV444 for video encoding.");
        }
        else
        {
            LOG_INFO("Using YUV420 for video encoding.");
        }
    }

    ~HardwareDisplayDrawingEngine() override
    {
        gShouldRender = false;
        {
            std::unique_lock lock(SurfaceMutex);
            Ready = 2;
            NotifyCV.notify_one();
        }
        EncodeThread.join();

        if (_screenTexture != nullptr)
        {
            SDL_DestroyTexture(_screenTexture);
        }
        if (_scaledScreenTexture != nullptr)
        {
            SDL_DestroyTexture(_scaledScreenTexture);
        }
        SDL_FreeFormat(_screenTextureFormat);
        SDL_DestroyRenderer(_sdlRenderer);

#ifdef ENABLE_VIDEO_RECORDING
        if (_codecContext || _audioCodecContext)
        {
            if (_codecContext)
            {
                encode_frame(_codecContext, nullptr, _formatContext, _videoStream);
            }
            if (_audioCodecContext && _swrContext && _audioFrame)
            {
                // Drain any remaining resampled audio samples from swrContext
                int frameSize = _audioCodecContext->frame_size > 0 ? _audioCodecContext->frame_size : 1024;
                while (true)
                {
                    int delay = swr_get_delay(_swrContext, 22050);
                    if (delay <= 0)
                    {
                        break;
                    }
                    av_frame_make_writable(_audioFrame);
                    _audioFrame->nb_samples = frameSize;
                    uint8_t** outData = _audioFrame->data;
                    int converted = swr_convert(_swrContext, outData, frameSize, nullptr, 0);
                    if (converted <= 0)
                    {
                        break;
                    }
                    _audioFrame->nb_samples = converted;
                    _audioFrame->pts = etd.nextAudioPts;
                    etd.nextAudioPts += converted;
                    encode_frame(_audioCodecContext, _audioFrame, _formatContext, _audioStream);
                }

                encode_frame(_audioCodecContext, nullptr, _formatContext, _audioStream);
            }

            if (_formatContext)
            {
                av_write_trailer(_formatContext);
            }

            if (_codecContext && (_codecContext->flags & AV_CODEC_FLAG_PASS1) && _codecContext->stats_out)
            {
                const char* envStats = getenv("OPENRCT2_ENCODER_STATS");
                if (envStats)
                {
                    FILE* f = fopen(envStats, "wb");
                    if (f)
                    {
                        fwrite(_codecContext->stats_out, 1, strlen(_codecContext->stats_out), f);
                        fclose(f);
                    }
                }
            }

            if (_audioCodecContext)
            {
                avcodec_free_context(&_audioCodecContext);
                av_frame_free(&_audioFrame);
            }
            if (_swrContext)
            {
                swr_free(&_swrContext);
            }

            if (_codecContext)
            {
                avcodec_free_context(&_codecContext);
                av_frame_free(&_frame);
            }

            if (_formatContext)
            {
                if (!(_formatContext->oformat->flags & AVFMT_NOFILE))
                    avio_closep(&_formatContext->pb);
                avformat_free_context(_formatContext);
            }
        }
        UnregisterFFmpegLogging();
#endif
    }

    void Initialise() override
    {
        _sdlRenderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | (_useVsync ? SDL_RENDERER_PRESENTVSYNC : 0));

        etd.NotifyCV = &NotifyCV;
        etd.Ready = &Ready;
        etd.SurfaceMutex = &SurfaceMutex;

        etd.yuv444 = _yuv444;

        EncodeThread = std::thread(EncodeThreadFunc, std::ref(etd));
    }

    void SetVSync(bool vsync) override
    {
        if (_useVsync != vsync)
        {
            _useVsync = vsync;
#if SDL_VERSION_ATLEAST(2, 0, 18)
            SDL_RenderSetVSync(_sdlRenderer, vsync ? 1 : 0);
#else
            SDL_DestroyRenderer(_sdlRenderer);
            _screenTexture = nullptr;
            _scaledScreenTexture = nullptr;
            Initialise();
            Resize(_uiContext->GetWidth(), _uiContext->GetHeight());
#endif
        }
    }

    void Resize(uint32_t width, uint32_t height) override
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        if (_screenTexture != nullptr)
        {
            SDL_DestroyTexture(_screenTexture);
        }
        SDL_FreeFormat(_screenTextureFormat);

        SDL_RendererInfo rendererInfo = {};
        int32_t result = SDL_GetRendererInfo(_sdlRenderer, &rendererInfo);
        if (result < 0)
        {
            LOG_WARNING("HWDisplayDrawingEngine::Resize error: %s", SDL_GetError());
            return;
        }
        uint32_t pixelFormat = SDL_PIXELFORMAT_UNKNOWN;
        for (uint32_t i = 0; i < rendererInfo.num_texture_formats; i++)
        {
            uint32_t format = rendererInfo.texture_formats[i];
            if (!SDL_ISPIXELFORMAT_FOURCC(format) && !SDL_ISPIXELFORMAT_INDEXED(format)
                && (pixelFormat == SDL_PIXELFORMAT_UNKNOWN || SDL_BYTESPERPIXEL(format) < SDL_BYTESPERPIXEL(pixelFormat)))
            {
                pixelFormat = format;
            }
        }

        ScaleQuality scaleQuality = GetContext()->GetUiContext().GetScaleQuality();
        if (scaleQuality == ScaleQuality::smoothNearestNeighbour)
        {
            scaleQuality = ScaleQuality::linear;
            smoothNN = true;
        }
        else
        {
            smoothNN = false;
        }

        if (smoothNN)
        {
            if (_scaledScreenTexture != nullptr)
            {
                SDL_DestroyTexture(_scaledScreenTexture);
            }

            char scaleQualityBuffer[4];
            snprintf(scaleQualityBuffer, sizeof(scaleQualityBuffer), "%d", static_cast<int32_t>(scaleQuality));
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

            _screenTexture = SDL_CreateTexture(_sdlRenderer, pixelFormat, SDL_TEXTUREACCESS_STREAMING, width, height);
            Guard::Assert(
                _screenTexture != nullptr, "Failed to create unscaled screen texture (%ux%u, pixelFormat = %u): %s", width,
                height, pixelFormat, SDL_GetError());

            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scaleQualityBuffer);

            uint32_t scale = std::ceil(Config::Get().general.windowScale);
            _scaledScreenTexture = SDL_CreateTexture(
                _sdlRenderer, pixelFormat, SDL_TEXTUREACCESS_TARGET, width * scale, height * scale);

            Guard::Assert(
                _scaledScreenTexture != nullptr,
                "Failed to create scaled screen texture (%ux%u, scale = %u, pixelFormat = %u): %s", width, height, scale,
                pixelFormat, SDL_GetError());
        }
        else
        {
            _screenTexture = SDL_CreateTexture(_sdlRenderer, pixelFormat, SDL_TEXTUREACCESS_STREAMING, width, height);
            Guard::Assert(
                _screenTexture != nullptr, "Failed to create screen texture (%ux%u, pixelFormat = %u): %s", width, height,
                pixelFormat, SDL_GetError());
        }

        uint32_t format;
        SDL_QueryTexture(_screenTexture, &format, nullptr, nullptr, nullptr);
        _screenTextureFormat = SDL_AllocFormat(format);

        X8DrawingEngine::Resize(width, height);

        // Update encode thread data with new dimensions
        etd.width = width;
        etd.height = height;
        etd.scale = Config::Get().general.windowScale;

        // Allocate A/B pixel buffers for double buffering
        etd.bufferSize = width * height * 4; // RGBA
        etd.pixelBufferA = std::make_unique<uint8_t[]>(etd.bufferSize);
        etd.pixelBufferB = std::make_unique<uint8_t[]>(etd.bufferSize);
        etd.activeBuffer = 0; // Start with buffer A
    }

private:
    void InitializeVideoEncoding()
    {
#ifdef ENABLE_VIDEO_RECORDING
        if (_videoInitialized)
            return;

        auto width = etd.width;
        auto height = etd.height;
        uint32_t scale = Config::Get().general.windowScale;
        uint32_t frame_width = width * scale;
        uint32_t frame_height = height * scale;

        if (frame_width <= 0 || frame_height <= 0 || (frame_width % 2) != 0 || (frame_height % 2) != 0)
        {
            LOG_FATAL("Invalid frame size: %dx%d (need to be larger than zero and even-sized)", frame_width, frame_height);
            return;
        }

        const char* envEncoder = getenv("OPENRCT2_ENCODER");
        const char* envPreset = getenv("OPENRCT2_ENCODER_PRESET");
        const char* envTune = getenv("OPENRCT2_ENCODER_TUNE");
        const char* envBitrate = getenv("OPENRCT2_ENCODER_BITRATE");
        const char* envGop = getenv("OPENRCT2_ENCODER_GOP");
        const char* envQuality = getenv("OPENRCT2_ENCODER_QUALITY");
        const char* envPass = getenv("OPENRCT2_ENCODER_PASS");

        const AVCodec* encoder = nullptr;
        if (envEncoder)
        {
            encoder = avcodec_find_encoder_by_name(envEncoder);
            if (!encoder)
            {
                LOG_ERROR("Requested encoder '%s' not found, falling back to default libvpx-vp9", envEncoder);
            }
        }

        if (!encoder)
        {
            encoder = avcodec_find_encoder_by_name("libvpx-vp9");
        }

        if (!encoder)
        {
            encoder = avcodec_find_encoder(AV_CODEC_ID_VP9);
        }

        if (!encoder)
        {
            encoder = avcodec_find_encoder(AV_CODEC_ID_VP8);
        }

        if (!encoder)
        {
            LOG_FATAL("No suitable encoder found");
            return;
        }

        LOG_INFO("Using encoder: %s", encoder->name);
        if (IsEncoderHW(encoder))
        {
            LOG_INFO("Encoder is hardware accelerated");
        }
        else
        {
            LOG_INFO("Encoder is NOT hardware accelerated");
            LogHWAlternatives(encoder->id);
        }

        using namespace std::string_literals;
        char* titleSeqName = getenv("TITLE_SEQUENCE_NAME");
        std::string titleSequenceNameStr;
        if (titleSeqName != nullptr)
        {
            titleSequenceNameStr = titleSeqName;
        }
        std::string videoName = "out"s + titleSequenceNameStr;
        RegisterFFmpegLogging(videoName);

        std::string filename = videoName + ".webm";

        if (avformat_alloc_output_context2(&_formatContext, nullptr, nullptr, filename.c_str()) < 0)
        {
            LOG_FATAL("Could not allocate output context");
            return;
        }

        // Check if the chosen encoder is compatible with webm
        if (avformat_query_codec(_formatContext->oformat, encoder->id, 1) != 1)
        {
            LOG_INFO("Encoder %s is not supported by WebM, switching to MKV container", encoder->name);
            avformat_free_context(_formatContext);
            _formatContext = nullptr;
            filename = videoName + ".mkv";
            if (avformat_alloc_output_context2(&_formatContext, nullptr, nullptr, filename.c_str()) < 0)
            {
                LOG_FATAL("Could not allocate output context for MKV");
                return;
            }
        }

        _videoStream = avformat_new_stream(_formatContext, nullptr);
        if (!_videoStream)
        {
            LOG_FATAL("Could not allocate stream");
            return;
        }

        _codecContext = avcodec_alloc_context3(encoder);
        if (!_codecContext)
        {
            LOG_FATAL("Could not allocate codec context");
            return;
        }

        _codecContext->width = frame_width;
        _codecContext->height = frame_height;
        _videoStream->time_base = { 1, static_cast<int>(FPS) };
        _codecContext->time_base = _videoStream->time_base;
        _codecContext->pix_fmt = _yuv444 ? AV_PIX_FMT_YUV444P : AV_PIX_FMT_YUV420P;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(62, 28, 100)
        const void* supportedPixelFormatConfigs = nullptr;
        avcodec_get_supported_config(
            _codecContext, encoder, AV_CODEC_CONFIG_PIX_FORMAT, 0, &supportedPixelFormatConfigs, nullptr);
        const auto* pixelFormats = static_cast<const enum AVPixelFormat*>(supportedPixelFormatConfigs);
#else
        const enum AVPixelFormat* pixelFormats = encoder->pix_fmts;
#endif
        if (pixelFormats != nullptr)
        {
            bool supported = false;
            for (const enum AVPixelFormat* p = pixelFormats; *p != AV_PIX_FMT_NONE; p++)
            {
                if (*p == _codecContext->pix_fmt)
                {
                    supported = true;
                    break;
                }
            }
            if (!supported)
            {
                enum AVPixelFormat bestFmt = AV_PIX_FMT_NONE;
                for (const enum AVPixelFormat* p = pixelFormats; *p != AV_PIX_FMT_NONE; p++)
                {
                    if (*p == AV_PIX_FMT_YUV420P || *p == AV_PIX_FMT_YUV444P || *p == AV_PIX_FMT_NV12 || *p == AV_PIX_FMT_P010)
                    {
                        bestFmt = *p;
                        break;
                    }
                }
                if (bestFmt == AV_PIX_FMT_NONE)
                {
                    bestFmt = pixelFormats[0];
                }
                LOG_INFO("Selected pixel format %d for encoder %s", bestFmt, encoder->name);
                _codecContext->pix_fmt = bestFmt;
            }
        }

        if (IsEncoderHW(encoder))
        {
            const AVCodecHWConfig* config = nullptr;
            for (int i = 0;; i++)
            {
                const AVCodecHWConfig* c = avcodec_get_hw_config(encoder, i);
                if (!c)
                    break;
                if (c->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                {
                    config = c;
                    break;
                }
            }
            if (config)
            {
                AVBufferRef* hw_device_ctx = nullptr;
                if (av_hwdevice_ctx_create(&hw_device_ctx, config->device_type, nullptr, nullptr, 0) >= 0)
                {
                    _codecContext->hw_device_ctx = hw_device_ctx;
                    LOG_INFO("Created HW device context for device type %d", config->device_type);
                }
            }
        }

        if (_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
            _codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (envPreset)
        {
            av_opt_set(_codecContext->priv_data, "preset", envPreset, 0);
        }
        if (envTune)
        {
            av_opt_set(_codecContext->priv_data, "tune", envTune, 0);
        }
        if (envBitrate)
        {
            _codecContext->bit_rate = atoll(envBitrate);
            _codecContext->rc_max_rate = _codecContext->bit_rate;
            _codecContext->rc_buffer_size = _codecContext->bit_rate * 2;
        }
        if (envGop)
        {
            _codecContext->gop_size = atoi(envGop);
        }
        if (envQuality)
        {
            av_opt_set(_codecContext->priv_data, "crf", envQuality, 0);
            _codecContext->global_quality = atoi(envQuality) * FF_QP2LAMBDA;
            _codecContext->flags |= AV_CODEC_FLAG_QSCALE;
        }
        if (envPass)
        {
            int pass = atoi(envPass);
            if (pass == 1)
            {
                _codecContext->flags |= AV_CODEC_FLAG_PASS1;
            }
            else if (pass == 2)
            {
                _codecContext->flags |= AV_CODEC_FLAG_PASS2;
                const char* envStats = getenv("OPENRCT2_ENCODER_STATS");
                if (envStats)
                {
                    FILE* f = fopen(envStats, "rb");
                    if (f)
                    {
                        fseek(f, 0, SEEK_END);
                        long size = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        char* stats = static_cast<char*>(av_malloc(size + 1));
                        if (stats)
                        {
                            if (fread(stats, 1, size, f) == static_cast<size_t>(size))
                            {
                                stats[size] = '\0';
                                _codecContext->stats_in = stats;
                            }
                            else
                            {
                                av_free(stats);
                            }
                        }
                        fclose(f);
                    }
                }
            }
        }
        if (!envPreset && encoder->id == AV_CODEC_ID_VP9)
        {
            av_opt_set(_codecContext->priv_data, "lossless", "1", 0);
        }

        if (avcodec_open2(_codecContext, encoder, nullptr) < 0)
        {
            LOG_FATAL("Could not open codec");
            return;
        }

        avcodec_parameters_from_context(_videoStream->codecpar, _codecContext);

        InitializeAudioEncoding();

        if (!(_formatContext->oformat->flags & AVFMT_NOFILE))
        {
            if (avio_open(&_formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0)
            {
                LOG_FATAL("Could not open '%s' for writing", filename.c_str());
                return;
            }
        }

        if (avformat_write_header(_formatContext, nullptr) < 0)
        {
            LOG_FATAL("Error occurred when opening output file");
            return;
        }

        _frame = av_frame_alloc();
        _frame->format = _codecContext->pix_fmt;
        _frame->width = _codecContext->width;
        _frame->height = _codecContext->height;

        if (av_frame_get_buffer(_frame, 0) < 0)
        {
            LOG_FATAL("Could not allocate the video frame data");
            return;
        }

        etd.formatContext = _formatContext;
        etd.codecContext = _codecContext;
        etd.videoStream = _videoStream;
        etd.frame = _frame;

        _videoInitialized = true;
#endif
    }

    void InitializeAudioEncoding()
    {
#ifdef ENABLE_VIDEO_RECORDING
        const char* envAudioEncoder = getenv("OPENRCT2_AUDIO_ENCODER");
        const char* envAudioBitrate = getenv("OPENRCT2_AUDIO_BITRATE");

        const AVCodec* audioEncoder = nullptr;
        if (envAudioEncoder)
        {
            audioEncoder = avcodec_find_encoder_by_name(envAudioEncoder);
            if (!audioEncoder)
            {
                LOG_ERROR("Requested audio encoder '%s' not found, falling back", envAudioEncoder);
            }
        }

        if (!audioEncoder)
        {
            // Try preferred encoders in order: FLAC, Opus, AAC, Vorbis
            static const char* preferredEncoders[] = { "flac", "libopus", "opus", "aac", "libvorbis", "vorbis" };
            for (const char* encName : preferredEncoders)
            {
                audioEncoder = avcodec_find_encoder_by_name(encName);
                if (audioEncoder && avformat_query_codec(_formatContext->oformat, audioEncoder->id, 1) == 1)
                {
                    break;
                }
                audioEncoder = nullptr;
            }
        }

        if (!audioEncoder)
        {
            // Fallback to format default audio codec
            AVCodecID defaultAudioCodec = _formatContext->oformat->audio_codec;
            if (defaultAudioCodec != AV_CODEC_ID_NONE)
            {
                audioEncoder = avcodec_find_encoder(defaultAudioCodec);
            }
        }

        if (!audioEncoder)
        {
            LOG_ERROR("Could not find suitable audio encoder for format context");
            return;
        }

        LOG_INFO("Using audio encoder: %s", audioEncoder->name);

        _audioStream = avformat_new_stream(_formatContext, nullptr);
        if (!_audioStream)
        {
            LOG_ERROR("Could not allocate audio stream");
            return;
        }

        _audioCodecContext = avcodec_alloc_context3(audioEncoder);
        if (!_audioCodecContext)
        {
            LOG_ERROR("Could not allocate audio codec context");
            return;
        }

        // Configure audio codec context
        _audioCodecContext->sample_rate = 44100;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(62, 28, 100)
        const void* supportedSampleRateConfigs = nullptr;
        avcodec_get_supported_config(
            _audioCodecContext, audioEncoder, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &supportedSampleRateConfigs, nullptr);
        const auto* supportedSampleRates = static_cast<const int*>(supportedSampleRateConfigs);
#else
        const int* supportedSampleRates = audioEncoder->supported_samplerates;
#endif
        if (supportedSampleRates)
        {
            bool rateSupported = false;
            for (const int* p = supportedSampleRates; *p != 0; p++)
            {
                if (*p == 44100)
                {
                    rateSupported = true;
                    break;
                }
            }
            if (!rateSupported)
            {
                _audioCodecContext->sample_rate = supportedSampleRates[0];
            }
        }

        // Always stereo layout
        AVChannelLayout chLayout = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&_audioCodecContext->ch_layout, &chLayout);

        // Select sample format
        enum AVSampleFormat sampleFmt = AV_SAMPLE_FMT_S16;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(62, 28, 100)
        const void* supportedSampleFormatConfigs = nullptr;
        avcodec_get_supported_config(
            _audioCodecContext, audioEncoder, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &supportedSampleFormatConfigs, nullptr);
        const auto* sampleFormats = static_cast<const enum AVSampleFormat*>(supportedSampleFormatConfigs);
#else
        const enum AVSampleFormat* sampleFormats = audioEncoder->sample_fmts;
#endif
        if (sampleFormats)
        {
            bool fmtSupported = false;
            for (const enum AVSampleFormat* p = sampleFormats; *p != AV_SAMPLE_FMT_NONE; p++)
            {
                if (*p == AV_SAMPLE_FMT_S16)
                {
                    fmtSupported = true;
                    sampleFmt = *p;
                    break;
                }
            }
            if (!fmtSupported)
            {
                for (const enum AVSampleFormat* p = sampleFormats; *p != AV_SAMPLE_FMT_NONE; p++)
                {
                    if (*p == AV_SAMPLE_FMT_S16P || *p == AV_SAMPLE_FMT_FLT || *p == AV_SAMPLE_FMT_FLTP || *p == AV_SAMPLE_FMT_S32)
                    {
                        fmtSupported = true;
                        sampleFmt = *p;
                        break;
                    }
                }
            }
            if (!fmtSupported)
            {
                sampleFmt = sampleFormats[0];
            }
        }
        _audioCodecContext->sample_fmt = sampleFmt;

        if (envAudioBitrate)
        {
            _audioCodecContext->bit_rate = atoll(envAudioBitrate);
        }
        else if (audioEncoder->id != AV_CODEC_ID_FLAC)
        {
            _audioCodecContext->bit_rate = 192000;
        }

        _audioStream->time_base = { 1, _audioCodecContext->sample_rate };
        _audioCodecContext->time_base = _audioStream->time_base;

        if (_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        {
            _audioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (avcodec_open2(_audioCodecContext, audioEncoder, nullptr) < 0)
        {
            LOG_ERROR("Could not open audio codec %s", audioEncoder->name);
            avcodec_free_context(&_audioCodecContext);
            _audioCodecContext = nullptr;
            return;
        }

        avcodec_parameters_from_context(_audioStream->codecpar, _audioCodecContext);

        // Setup resampler from OpenRCT2 mixer (22050Hz S16 Stereo) to codec target
        AVChannelLayout srcLayout = AV_CHANNEL_LAYOUT_STEREO;
        swr_alloc_set_opts2(
            &_swrContext,
            &_audioCodecContext->ch_layout,
            _audioCodecContext->sample_fmt,
            _audioCodecContext->sample_rate,
            &srcLayout,
            AV_SAMPLE_FMT_S16,
            22050,
            0,
            nullptr);

        if (!_swrContext || swr_init(_swrContext) < 0)
        {
            LOG_ERROR("Failed to initialize audio resampler");
            if (_swrContext)
            {
                swr_free(&_swrContext);
            }
            avcodec_free_context(&_audioCodecContext);
            _audioCodecContext = nullptr;
            return;
        }

        _audioFrame = av_frame_alloc();
        _audioFrame->format = _audioCodecContext->sample_fmt;
        av_channel_layout_copy(&_audioFrame->ch_layout, &_audioCodecContext->ch_layout);
        _audioFrame->sample_rate = _audioCodecContext->sample_rate;
        _audioFrame->nb_samples = _audioCodecContext->frame_size > 0 ? _audioCodecContext->frame_size : 8192;

        if (av_frame_get_buffer(_audioFrame, 0) < 0)
        {
            LOG_ERROR("Could not allocate audio frame buffer");
            av_frame_free(&_audioFrame);
            swr_free(&_swrContext);
            avcodec_free_context(&_audioCodecContext);
            _audioCodecContext = nullptr;
            return;
        }

        etd.audioCodecContext = _audioCodecContext;
        etd.audioStream = _audioStream;
        etd.audioFrame = _audioFrame;
        etd.swrContext = _swrContext;
        etd.nextAudioPts = 0;
        etd.audioSamplesWritten = 0;
#endif
    }

    void SetPalette(const GamePalette& palette) override
    {
        if (_screenTextureFormat != nullptr)
        {
            for (int32_t i = 0; i < 256; i++)
            {
                _paletteHWMapped[i] = SDL_MapRGB(_screenTextureFormat, palette[i].red, palette[i].green, palette[i].blue);
            }

            if (Config::Get().general.enableLightFx)
            {
                auto& lightPalette = LightFx::GetPalette();
                for (int32_t i = 0; i < 256; i++)
                {
                    const auto& src = lightPalette[i];
                    _lightPaletteHWMapped[i] = SDL_MapRGBA(_screenTextureFormat, src.red, src.green, src.blue, src.alpha);
                }
            }
        }
    }

    void BeginDraw() override
    {
        X8DrawingEngine::BeginDraw();
    }

    void EndDraw() override
    {
        X8DrawingEngine::EndDraw();

        Display();
    }

protected:
    void OnDrawDirtyBlock(int32_t left, int32_t top, int32_t right, int32_t bottom) override
    {
        if (gShowDirtyVisuals)
        {
            const auto columns = ((right - left) + (_invalidationGrid.getBlockWidth() - 1)) / _invalidationGrid.getBlockWidth();
            const auto rows = ((bottom - top) + (_invalidationGrid.getBlockHeight() - 1)) / _invalidationGrid.getBlockHeight();
            const auto firstRow = top / _invalidationGrid.getBlockHeight();
            const auto firstColumn = left / _invalidationGrid.getBlockWidth();

            for (uint32_t y = 0; y < rows; y++)
            {
                for (uint32_t x = 0; x < columns; x++)
                {
                    SetDirtyVisualTime(firstColumn + x, firstRow + y, gCurrentRealTimeTicks + kDirtyVisualTime);
                }
            }
        }
    }

private:
    void Display()
    {
        auto* viewport = WindowGetViewport(WindowGetMain());

        if (Config::Get().general.enableLightFx && viewport != nullptr)
        {
            void* pixels;
            int32_t pitch;
            if (SDL_LockTexture(_screenTexture, nullptr, &pixels, &pitch) == 0)
            {
                LightFx::RenderToTexture(
                    *viewport, pixels, pitch, _bits, _width, _height, _paletteHWMapped, _lightPaletteHWMapped);
                SDL_UnlockTexture(_screenTexture);
            }
        }
        else
        {
            CopyBitsToTexture(
                _screenTexture, _bits, static_cast<int32_t>(_width), static_cast<int32_t>(_height), _paletteHWMapped);
        }
        if (smoothNN)
        {
            SDL_SetRenderTarget(_sdlRenderer, _scaledScreenTexture);
            SDL_RenderCopy(_sdlRenderer, _screenTexture, nullptr, nullptr);

            SDL_SetRenderTarget(_sdlRenderer, nullptr);
            SDL_RenderCopy(_sdlRenderer, _scaledScreenTexture, nullptr, nullptr);
        }
        else
        {
            SDL_RenderCopy(_sdlRenderer, _screenTexture, nullptr, nullptr);
        }

        if (gShowDirtyVisuals)
        {
            RenderDirtyVisuals();
        }

        if (gShouldRender)
        {
            // Initialize video encoding only when title sequence starts playing
            // and UI elements are hidden
            if (!_videoInitialized)
            {
                // Check if we're in title sequence scene and if the title sequence has started
                // We need to ensure UI elements are no longer visible before starting recording
                auto* player = static_cast<ITitleSequencePlayer*>(TitleGetSequencePlayer());
                // Only start recording if the player exists and has advanced past the initial setup
                // Position > 1 ensures we've moved past initial loading commands
                if (player != nullptr && player->GetCurrentPosition() > 1)
                {
                    // Additionally, close all title-related UI windows to ensure clean recording
                    auto* windowMgr = Ui::GetWindowManager();
                    if (windowMgr != nullptr)
                    {
                        windowMgr->CloseByClass(WindowClass::titleLogo);
                        windowMgr->CloseByClass(WindowClass::titleMenu);
                        windowMgr->CloseByClass(WindowClass::titleVersion);
                        windowMgr->CloseByClass(WindowClass::titleExit);
                        windowMgr->CloseByClass(WindowClass::titleOptions);
                    }
                    InitializeVideoEncoding();
                }
            }

            // Only proceed with encoding if video is initialized
            if (_videoInitialized)
            {
#ifdef ENABLE_VIDEO_RECORDING
                auto* player = static_cast<ITitleSequencePlayer*>(TitleGetSequencePlayer());
                if (player != nullptr && player->PopCommandExecutedSignal())
                {
                    etd.forceKeyframe.store(true);
                }
#endif

                // Determine which buffer to write to (opposite of the one being encoded)
                int writeBuffer = (etd.activeBuffer == 0) ? 1 : 0;
                uint8_t* writePixelBuffer = (writeBuffer == 0) ? etd.pixelBufferA.get() : etd.pixelBufferB.get();

                if (writePixelBuffer && etd.bufferSize > 0)
                {
                    int pitch;
                    void* pixels;
                    if (SDL_LockTexture(_screenTexture, nullptr, &pixels, &pitch) == 0)
                    {
                        // Calculate how much data to copy based on actual pitch
                        const size_t rowSize = std::min(static_cast<size_t>(etd.width * 4), static_cast<size_t>(pitch));
                        const uint8_t* srcPixels = static_cast<const uint8_t*>(pixels);

                        // Copy row by row to handle pitch correctly
                        for (uint32_t y = 0; y < etd.height; ++y)
                        {
                            std::memcpy(writePixelBuffer + y * etd.width * 4, srcPixels + y * pitch, rowSize);
                        }

                        SDL_UnlockTexture(_screenTexture);

                        // Fetch audio chunk for this video frame into write audio buffer
                        if (_audioCodecContext != nullptr)
                        {
                            auto& writeAudioBuf = (writeBuffer == 0) ? etd.audioBufferA : etd.audioBufferB;

                            _audioSampleAcc += 22050.0 / static_cast<double>(FPS);
                            int samplesToFetch = static_cast<int>(_audioSampleAcc);
                            _audioSampleAcc -= samplesToFetch;

                            size_t audioBytes = samplesToFetch * 4; // 2 channels * 2 bytes per S16 sample
                            writeAudioBuf.resize(audioBytes);

                            auto* mixer = GetContext()->GetAudioContext().GetMixer();
                            if (mixer != nullptr)
                            {
                                mixer->GetNextAudioChunk(writeAudioBuf.data(), audioBytes);
                            }
                            else
                            {
                                std::fill(writeAudioBuf.begin(), writeAudioBuf.end(), 0);
                            }
                        }

                        // Swap buffers atomically
                        etd.activeBuffer = writeBuffer;

                        // Notify encoding thread
                        std::unique_lock lock(SurfaceMutex);
                        Ready = 1;
                        NotifyCV.notify_one();
                    }
                    else
                    {
                        LOG_WARNING("Failed to lock texture for encoding: %s", SDL_GetError());
                    }
                }
            }
        }
        SDL_RenderPresent(_sdlRenderer);
    }

    void CopyBitsToTexture(SDL_Texture* texture, PaletteIndex* src, int32_t width, int32_t height, const uint32_t* palette)
    {
        void* pixels;
        int32_t pitch;
        if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) == 0)
        {
            int32_t padding = pitch - (width * 4);
            if (pitch == width * 4)
            {
                uint32_t* dst = static_cast<uint32_t*>(pixels);
                for (int32_t i = width * height; i > 0; i--)
                {
                    *dst++ = palette[EnumValue(*src++)];
                }
            }
            else
            {
                if (pitch == (width * 2) + padding)
                {
                    uint16_t* dst = static_cast<uint16_t*>(pixels);
                    for (int32_t y = height; y > 0; y--)
                    {
                        for (int32_t x = width; x > 0; x--)
                        {
                            const uint8_t lower = *reinterpret_cast<const uint8_t*>(&palette[EnumValue(*src++)]);
                            const uint8_t upper = *reinterpret_cast<const uint8_t*>(&palette[EnumValue(*src++)]);
                            *dst++ = (lower << 8) | upper;
                        }
                        dst = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(dst) + padding);
                    }
                }
                else if (pitch == width + padding)
                {
                    uint8_t* dst = static_cast<uint8_t*>(pixels);
                    for (int32_t y = height; y > 0; y--)
                    {
                        for (int32_t x = width; x > 0; x--)
                        {
                            *dst++ = *reinterpret_cast<const uint8_t*>(&palette[EnumValue(*src++)]);
                        }
                        dst += padding;
                    }
                }
            }
            SDL_UnlockTexture(texture);
        }
    }

    uint32_t GetDirtyVisualTime(uint32_t x, uint32_t y)
    {
        uint32_t result = 0;
        uint32_t i = y * _invalidationGrid.getColumnCount() + x;
        if (_dirtyVisualsTime.size() > i)
        {
            result = _dirtyVisualsTime[i];
        }
        return result;
    }

    void SetDirtyVisualTime(uint32_t x, uint32_t y, uint32_t value)
    {
        const auto rows = _invalidationGrid.getRowCount();
        const auto columns = _invalidationGrid.getColumnCount();

        _dirtyVisualsTime.resize(rows * columns);

        uint32_t i = y * _invalidationGrid.getColumnCount() + x;
        if (_dirtyVisualsTime.size() > i)
        {
            _dirtyVisualsTime[i] = value;
        }
    }

    void RenderDirtyVisuals()
    {
        int windowX, windowY, renderX, renderY;
        SDL_GetWindowSize(_window, &windowX, &windowY);
        SDL_GetRendererOutputSize(_sdlRenderer, &renderX, &renderY);

        float scaleX = Config::Get().general.windowScale * renderX / static_cast<float>(windowX);
        float scaleY = Config::Get().general.windowScale * renderY / static_cast<float>(windowY);

        SDL_SetRenderDrawBlendMode(_sdlRenderer, SDL_BLENDMODE_BLEND);
        for (uint32_t y = 0; y < _invalidationGrid.getRowCount(); y++)
        {
            for (uint32_t x = 0; x < _invalidationGrid.getColumnCount(); x++)
            {
                const auto timeEnd = GetDirtyVisualTime(x, y);
                const auto timeLeft = gCurrentRealTimeTicks < timeEnd ? timeEnd - gCurrentRealTimeTicks : 0;
                if (timeLeft > 0)
                {
                    uint8_t alpha = timeLeft * kDirtyRegionAlpha / kDirtyVisualTime;
                    SDL_Rect ddRect;
                    ddRect.x = static_cast<int32_t>(x * _invalidationGrid.getBlockWidth() * scaleX);
                    ddRect.y = static_cast<int32_t>(y * _invalidationGrid.getBlockHeight() * scaleY);
                    ddRect.w = static_cast<int32_t>(_invalidationGrid.getBlockWidth() * scaleX);
                    ddRect.h = static_cast<int32_t>(_invalidationGrid.getBlockHeight() * scaleY);

                    SDL_SetRenderDrawColor(_sdlRenderer, 255, 255, 255, alpha);
                    SDL_RenderFillRect(_sdlRenderer, &ddRect);
                }
            }
        }
    }
};

std::unique_ptr<IDrawingEngine> Ui::CreateHardwareDisplayDrawingEngine(IUiContext& uiContext)
{
    return std::make_unique<HardwareDisplayDrawingEngine>(uiContext);
}
