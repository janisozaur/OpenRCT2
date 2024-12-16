/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AudioContext.h"
#include "AudioFormat.h"
#include "SDLAudioSource.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <algorithm>
#include <cmath>
#include <openrct2/audio/AudioSource.h>

namespace OpenRCT2::Audio
{
    template<typename AudioSource_ = SDLAudioSource>
    class AudioChannelImpl final : public ISDLAudioChannel
    {
        static_assert(std::is_base_of_v<IAudioSource, AudioSource_>);

    private:
        AudioSource_* _source = nullptr;
        ALuint _buffer = 0;
        ALuint _source_id = 0;

        MixerGroup _group = MixerGroup::Sound;
        double _rate = 0;
        uint64_t _offset = 0;
        int32_t _loop = 0;

        int32_t _volume = 1;
        float _volume_l = 0.f;
        float _volume_r = 0.f;
        float _oldvolume_l = 0.f;
        float _oldvolume_r = 0.f;
        int32_t _oldvolume = 0;
        float _pan = 0;

        bool _stopping = false;
        bool _done = true;
        bool _deleteondone = false;

    public:
        AudioChannelImpl()
        {
            alGenBuffers(1, &_buffer);
            alGenSources(1, &_source_id);

            AudioChannelImpl::SetRate(1);
            AudioChannelImpl::SetVolume(kMixerVolumeMax);
            AudioChannelImpl::SetPan(0.5f);
        }

        ~AudioChannelImpl() override
        {
            if (_buffer)
            {
                alDeleteBuffers(1, &_buffer);
            }
            if (_source_id)
            {
                alDeleteSources(1, &_source_id);
            }
        }

        [[nodiscard]] IAudioSource* GetSource() const override
        {
            return _source;
        }

        [[nodiscard]] SpeexResamplerState* GetResampler() const override
        {
            return nullptr;
        }

        void SetResampler(SpeexResamplerState* value) override
        {
        }

        [[nodiscard]] MixerGroup GetGroup() const override
        {
            return _group;
        }

        void SetGroup(MixerGroup group) override
        {
            _group = group;
        }

        [[nodiscard]] double GetRate() const override
        {
            return _rate;
        }

        void SetRate(double rate) override
        {
            _rate = std::max(0.001, rate);
        }

        [[nodiscard]] uint64_t GetOffset() const override
        {
            return _offset;
        }

        bool SetOffset(uint64_t offset) override
        {
            if (_source != nullptr && offset < _source->GetLength())
            {
                AudioFormat format = _source->GetFormat();
                int32_t samplesize = format.channels * format.BytesPerSample();
                _offset = (offset / samplesize) * samplesize;
                return true;
            }
            return false;
        }

        [[nodiscard]] int32_t GetLoop() const override
        {
            return _loop;
        }

        void SetLoop(int32_t value) override
        {
            _loop = value;
        }

        [[nodiscard]] int32_t GetVolume() const override
        {
            return _volume;
        }

        [[nodiscard]] float GetVolumeL() const override
        {
            return _volume_l;
        }

        [[nodiscard]] float GetVolumeR() const override
        {
            return _volume_r;
        }

        [[nodiscard]] float GetOldVolumeL() const override
        {
            return _oldvolume_l;
        }

        [[nodiscard]] float GetOldVolumeR() const override
        {
            return _oldvolume_r;
        }

        [[nodiscard]] int32_t GetOldVolume() const override
        {
            return _oldvolume;
        }

        void SetVolume(int32_t volume) override
        {
            _volume = std::clamp(volume, 0, kMixerVolumeMax);
        }

        [[nodiscard]] float GetPan() const override
        {
            return _pan;
        }

        void SetPan(float pan) override
        {
            _pan = std::clamp(pan, 0.0f, 1.0f);
            double decibels = (std::abs(_pan - 0.5) * 2.0) * 100.0;
            double attenuation = pow(10, decibels / 20.0);
            if (_pan <= 0.5)
            {
                _volume_l = 1.0;
                _volume_r = static_cast<float>(1.0 / attenuation);
            }
            else
            {
                _volume_r = 1.0;
                _volume_l = static_cast<float>(1.0 / attenuation);
            }
        }

        [[nodiscard]] bool IsStopping() const override
        {
            return _stopping;
        }

        void SetStopping(bool value) final override
        {
            _stopping = value;
        }

        [[nodiscard]] bool IsDone() const override
        {
            return _done;
        }

        void SetDone(bool value) override
        {
            _done = value;
        }

        [[nodiscard]] bool DeleteOnDone() const override
        {
            return _deleteondone;
        }

        void SetDeleteOnDone(bool value) override
        {
            _deleteondone = value;
        }

        [[nodiscard]] bool IsPlaying() const override
        {
            return !_done;
        }

        void Play(IAudioSource* source, int32_t loop) override
        {
            _source = static_cast<AudioSource_*>(source);
            _loop = loop;
            _offset = 0;
            _done = false;

            // Configure OpenAL source
            alSourcei(_source_id, AL_LOOPING, loop == kMixerLoopInfinite ? AL_TRUE : AL_FALSE);
            alSourcef(_source_id, AL_GAIN, static_cast<float>(_volume) / kMixerVolumeMax);
            alSource3f(_source_id, AL_POSITION, _pan - 0.5f, 0.0f, 0.0f);

            // Buffer the audio data
            auto format = _source->GetFormat();
            ALenum alFormat = (format.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

            // Read the entire source into buffer
            std::vector<uint8_t> data(_source->GetLength());
            _source->Read(data.data(), 0, data.size());

            alBufferData(_buffer, alFormat, data.data(), data.size(), format.freq);
            alSourcei(_source_id, AL_BUFFER, _buffer);
            alSourcePlay(_source_id);
        }

        void Stop() override
        {
            alSourceStop(_source_id);
            SetStopping(true);
        }

        void UpdateOldVolume() override
        {
            _oldvolume = _volume;
            _oldvolume_l = _volume_l;
            _oldvolume_r = _volume_r;
        }

        [[nodiscard]] AudioFormat GetFormat() const override
        {
            AudioFormat result = {};
            if (_source != nullptr)
            {
                result = _source->GetFormat();
            }
            return result;
        }

        size_t Read(void* dst, size_t len) override
        {
            // OpenAL handles the actual audio streaming, so we just need to check state
            ALint state;
            alGetSourcei(_source_id, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING)
            {
                _done = true;
            }
            return 0;
        }
    };

    ISDLAudioChannel* AudioChannel::Create()
    {
        return new (std::nothrow) AudioChannelImpl();
    }
} // namespace OpenRCT2::Audio
