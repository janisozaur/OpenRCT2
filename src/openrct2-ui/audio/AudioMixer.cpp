/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/
#include "AudioMixer.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <algorithm>
#include <iterator>
#include <openrct2/OpenRCT2.h>
#include <openrct2/config/Config.h>

using namespace OpenRCT2::Audio;

AudioMixer::~AudioMixer()
{
    Close();
}

void AudioMixer::Init(const char* device)
{
    ALCdevice* alcDevice = alcOpenDevice(device);
    ALCcontext* alcContext = alcCreateContext(alcDevice, nullptr);
    alcMakeContextCurrent(alcContext);

    // Configure OpenAL settings
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    // Initialize buffer pools and source management
}

void AudioMixer::Close()
{
    // Free channels
    Lock();
    _channels.clear();
    Unlock();

    SDL_CloseAudioDevice(_deviceId);

    // Free buffers
    _channelBuffer.clear();
    _channelBuffer.shrink_to_fit();
    _convertBuffer.clear();
    _convertBuffer.shrink_to_fit();
    _effectBuffer.clear();
    _effectBuffer.shrink_to_fit();
}

void AudioMixer::Lock()
{
    SDL_LockAudioDevice(_deviceId);
}

void AudioMixer::Unlock()
{
    SDL_UnlockAudioDevice(_deviceId);
}

std::shared_ptr<IAudioChannel> AudioMixer::Play(IAudioSource* source, int32_t loop, bool deleteondone)
{
    Lock();
    auto channel = std::shared_ptr<ISDLAudioChannel>(AudioChannel::Create());
    if (channel != nullptr)
    {
        channel->Play(source, loop);
        channel->SetDeleteOnDone(deleteondone);
        _channels.push_back(channel);
    }
    Unlock();
    return channel;
}

SDLAudioSource* AudioMixer::AddSource(std::unique_ptr<SDLAudioSource> source)
{
    std::lock_guard<std::mutex> guard(_mutex);
    if (source != nullptr)
    {
        _sources.push_back(std::move(source));
        return _sources.back().get();
    }
    return nullptr;
}

void AudioMixer::RemoveReleasedSources()
{
    std::lock_guard<std::mutex> guard(_mutex);
    _sources.erase(
        std::remove_if(
            _sources.begin(), _sources.end(),
            [](std::unique_ptr<SDLAudioSource>& source) {
                {
                    return source->IsReleased();
                }
            }),
        _sources.end());
}

const AudioFormat& AudioMixer::GetFormat() const
{
    return _format;
}

void AudioMixer::GetNextAudioChunk(uint8_t* dst, size_t length)
{
    UpdateAdjustedSound();

    // Zero the output buffer
    std::fill_n(dst, length, 0);

    // Mix channels onto output buffer
    auto it = _channels.begin();
    while (it != _channels.end())
    {
        auto& channel = *it;
        auto channelSource = channel->GetSource();
        auto channelSourceReleased = channelSource == nullptr || channelSource->IsReleased();
        if (channelSourceReleased || (channel->IsDone() && channel->DeleteOnDone()) || channel->IsStopping())
        {
            channel->SetDone(true);
            it = _channels.erase(it);
        }
        else
        {
            auto group = channel->GetGroup();
            if ((group != MixerGroup::Sound || Config::Get().sound.SoundEnabled) && Config::Get().sound.MasterSoundEnabled
                && Config::Get().sound.MasterVolume != 0)
            {
                MixChannel(channel.get(), dst, length);
            }
            it++;
        }
    }
}

void AudioMixer::UpdateAdjustedSound()
{
    // Did the volume level get changed? Recalculate level in this case.
    if (_settingSoundVolume != Config::Get().sound.SoundVolume)
    {
        _settingSoundVolume = Config::Get().sound.SoundVolume;
        _adjustSoundVolume = powf(static_cast<float>(_settingSoundVolume) / 100.f, 10.f / 6.f);
    }
    if (_settingMusicVolume != Config::Get().sound.AudioFocus)
    {
        _settingMusicVolume = Config::Get().sound.AudioFocus;
        _adjustMusicVolume = powf(static_cast<float>(_settingMusicVolume) / 100.f, 10.f / 6.f);
    }
}

void AudioMixer::MixChannel(ISDLAudioChannel* channel, uint8_t* data, size_t length)
{
    ALuint source;
    alGenSources(1, &source);

    // Configure source properties
    alSource3f(
        source, AL_POSITION,
        channel->GetPan() * 2.0f - 1.0f, // Convert pan to X position
        0.0f, 0.0f);
    alSourcef(source, AL_GAIN, channel->GetVolume());

    // Queue audio data
    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, AL_FORMAT_STEREO16, data, length, _format.freq);
    alSourceQueueBuffers(source, 1, &buffer);
}

/**
 * Resample the given buffer into _effectBuffer.
 * Assumes that srcBuffer is the same format as _format.
 */
size_t AudioMixer::ApplyResample(
    ISDLAudioChannel* channel, const void* srcBuffer, int32_t srcSamples, int32_t dstSamples, int32_t inRate, int32_t outRate)
{
    int32_t byteRate = _format.GetByteRate();

    // Create OpenAL buffer for resampling
    ALuint buffer;
    alGenBuffers(1, &buffer);

    // Load source data into buffer with original format
    alBufferData(
        buffer, _format.channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, srcBuffer, srcSamples * byteRate, inRate);

    // OpenAL will handle resampling when playing at different frequency
    ALsizei size;
    ALsizei freq;
    alGetBufferi(buffer, AL_SIZE, &size);
    alGetBufferi(buffer, AL_FREQUENCY, &freq);

    // Get resampled data
    std::vector<ALshort> resampledData(dstSamples * _format.channels);
    alBufferData(
        buffer, _format.channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, resampledData.data(), dstSamples * byteRate,
        outRate);

    // Copy resampled data to effect buffer
    memcpy(_effectBuffer.data(), resampledData.data(), dstSamples * byteRate);

    alDeleteBuffers(1, &buffer);

    return dstSamples * byteRate;
}

void AudioMixer::ApplyPan(const IAudioChannel* channel, void* buffer, size_t len, size_t sampleSize)
{
    if (channel->GetPan() != 0.5f && _format.channels == 2)
    {
        switch (_format.format)
        {
            case AUDIO_S16SYS:
                EffectPanS16(channel, static_cast<int16_t*>(buffer), static_cast<int32_t>(len / sampleSize));
                break;
            case AUDIO_U8:
                EffectPanU8(channel, static_cast<uint8_t*>(buffer), static_cast<int32_t>(len / sampleSize));
                break;
        }
    }
}

void AudioMixer::SetVolume(float volume)
{
    _volume = volume;
    // Set master gain in OpenAL
    alListenerf(
        AL_GAIN,
        _volume
            * (Config::Get().sound.MasterSoundEnabled ? (static_cast<float>(Config::Get().sound.MasterVolume) / 100.0f)
                                                      : 0.0f));
}

int32_t AudioMixer::ApplyVolume(const IAudioChannel* channel, void* buffer, size_t len)
{
    float volumeAdjust = _volume;
    volumeAdjust *= Config::Get().sound.MasterSoundEnabled ? (static_cast<float>(Config::Get().sound.MasterVolume) / 100.0f)
                                                           : 0.0f;

    switch (channel->GetGroup())
    {
        case MixerGroup::Sound:
            volumeAdjust *= _adjustSoundVolume;

            // Cap sound volume on title screen so music is more audible
            if (gScreenFlags & SCREEN_FLAGS_TITLE_DEMO)
            {
                volumeAdjust = std::min(volumeAdjust, 0.75f);
            }
            break;
        case MixerGroup::RideMusic:
        case MixerGroup::TitleMusic:
            volumeAdjust *= _adjustMusicVolume;
            break;
    }

    // Set per-source gain in OpenAL
    ALuint source = channel->GetSource();
    alSourcef(source, AL_GAIN, volumeAdjust);

    return static_cast<int32_t>(volumeAdjust * kMixerVolumeMax);
}

void AudioMixer::EffectPanS16(const IAudioChannel* channel, int16_t* data, int32_t length)
{
    const float dt = 1.0f / static_cast<float>(length * 2.0f);
    float volumeL = channel->GetOldVolumeL();
    float volumeR = channel->GetOldVolumeR();
    const float d_left = dt * (channel->GetVolumeL() - channel->GetOldVolumeL());
    const float d_right = dt * (channel->GetVolumeR() - channel->GetOldVolumeR());

    for (int32_t i = 0; i < length * 2; i += 2)
    {
        data[i + 0] = static_cast<int16_t>(volumeL * static_cast<float>(data[i + 0]));
        data[i + 1] = static_cast<int16_t>(volumeR * static_cast<float>(data[i + 1]));
        volumeL += d_left;
        volumeR += d_right;
    }
}

void AudioMixer::EffectPanU8(const IAudioChannel* channel, uint8_t* data, int32_t length)
{
    float volumeL = channel->GetVolumeL();
    float volumeR = channel->GetVolumeR();
    float oldVolumeL = channel->GetOldVolumeL();
    float oldVolumeR = channel->GetOldVolumeR();

    for (int32_t i = 0; i < length * 2; i += 2)
    {
        float t = static_cast<float>(i) / static_cast<float>(length * 2.0f);
        data[i] = static_cast<uint8_t>(data[i] * ((1.0 - t) * oldVolumeL + t * volumeL));
        data[i + 1] = static_cast<uint8_t>(data[i + 1] * ((1.0 - t) * oldVolumeR + t * volumeR));
    }
}

void AudioMixer::EffectFadeS16(int16_t* data, int32_t length, int32_t startvolume, int32_t endvolume)
{
    static_assert(SDL_MIX_MAXVOLUME == kMixerVolumeMax, "Max volume differs between OpenRCT2 and SDL2");

    float startvolume_f = static_cast<float>(startvolume) / SDL_MIX_MAXVOLUME;
    float endvolume_f = static_cast<float>(endvolume) / SDL_MIX_MAXVOLUME;
    for (int32_t i = 0; i < length; i++)
    {
        float t = static_cast<float>(i) / length;
        data[i] = static_cast<int16_t>(data[i] * ((1.0f - t) * startvolume_f + t * endvolume_f));
    }
}

void AudioMixer::EffectFadeU8(uint8_t* data, int32_t length, int32_t startvolume, int32_t endvolume)
{
    static_assert(SDL_MIX_MAXVOLUME == kMixerVolumeMax, "Max volume differs between OpenRCT2 and SDL2");

    float startvolume_f = static_cast<float>(startvolume) / SDL_MIX_MAXVOLUME;
    float endvolume_f = static_cast<float>(endvolume) / SDL_MIX_MAXVOLUME;
    for (int32_t i = 0; i < length; i++)
    {
        float t = static_cast<float>(i) / length;
        data[i] = static_cast<uint8_t>(data[i] * ((1.0f - t) * startvolume_f + t * endvolume_f));
    }
}

bool AudioMixer::Convert(SDL_AudioCVT* cvt, const void* src, size_t len)
{
    // tofix: there seems to be an issue with converting audio using SDL_ConvertAudio in the callback vs preconverted,
    // can cause pops and static depending on sample rate and channels
    bool result = false;
    if (len != 0 && cvt->len_mult != 0)
    {
        size_t reqConvertBufferCapacity = len * cvt->len_mult;
        _convertBuffer.resize(reqConvertBufferCapacity);
        std::copy_n(static_cast<const uint8_t*>(src), len, _convertBuffer.data());

        cvt->len = static_cast<int32_t>(len);
        cvt->buf = static_cast<uint8_t*>(_convertBuffer.data());
        if (SDL_ConvertAudio(cvt) >= 0)
        {
            result = true;
        }
    }
    return result;
}
