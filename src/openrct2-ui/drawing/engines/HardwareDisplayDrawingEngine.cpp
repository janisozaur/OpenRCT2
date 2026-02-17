/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "DrawingEngineFactory.hpp"

#include <SDL.h>
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
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}
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
        av_packet_free(&pkt);
        if (ret < 0)
        {
            LOG_ERROR("Error while writing video frame\n");
            return ret;
        }

        printf(keyframe ? "K" : ".");
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
        if (_codecContext)
        {
            encode_frame(_codecContext, nullptr, _formatContext, _videoStream);
            av_write_trailer(_formatContext);

            avcodec_free_context(&_codecContext);
            av_frame_free(&_frame);
            if (!(_formatContext->oformat->flags & AVFMT_NOFILE))
                avio_closep(&_formatContext->pb);
            avformat_free_context(_formatContext);
        }
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
        if (scaleQuality == ScaleQuality::SmoothNearestNeighbour)
        {
            scaleQuality = ScaleQuality::Linear;
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
        std::string filename = "out"s + titleSequenceNameStr + ".webm";

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
            filename = "out"s + titleSequenceNameStr + ".mkv";
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

        if (_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
            _codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (envPreset)
        {
            av_opt_set(_codecContext->priv_data, "preset", envPreset, 0);
        }
        else if (encoder->id == AV_CODEC_ID_VP9)
        {
            av_opt_set(_codecContext->priv_data, "lossless", "1", 0);
        }

        if (avcodec_open2(_codecContext, encoder, nullptr) < 0)
        {
            LOG_FATAL("Could not open codec");
            return;
        }

        avcodec_parameters_from_context(_videoStream->codecpar, _codecContext);

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
