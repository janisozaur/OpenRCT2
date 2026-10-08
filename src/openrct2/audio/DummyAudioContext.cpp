/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AudioContext.h"
#include "AudioMixer.h"

namespace OpenRCT2::Audio
{
    class DummyAudioMixer final : public IAudioMixer
    {
    public:
        void Init(const char* /* device */) override
        {
        }
        void Close() override
        {
        }
        void Lock() override
        {
        }
        void Unlock() override
        {
        }
        std::shared_ptr<IAudioChannel> Play(IAudioSource* /* source */, int32_t /* loop */, bool /* deleteondone */) override
        {
            return nullptr;
        }
        void SetVolume(float /* volume */) override
        {
        }
        void GetNextAudioChunk(uint8_t* dst, size_t length) override
        {
            std::fill_n(dst, length, 0);
        }
        int32_t GetOutputSampleRate() const override
        {
            return 22050;
        }
        void SetAudioCaptureCallback(std::function<void(const uint8_t*, size_t)> /* callback */) override
        {
        }
    };

    class DummyAudioContext final : public IAudioContext
    {
    private:
        DummyAudioMixer _mixer;

    public:
        IAudioMixer* GetMixer() override
        {
            return &_mixer;
        }

        std::vector<std::string> GetOutputDevices() override
        {
            return std::vector<std::string>();
        }
        void SetOutputDevice(const std::string& /*deviceName*/) override
        {
        }

        IAudioSource* CreateStreamFromCSS(std::unique_ptr<IStream> /* stream */, uint32_t /* index */) override
        {
            return nullptr;
        }

        IAudioSource* CreateStreamFromWAV(std::unique_ptr<IStream>) override
        {
            return nullptr;
        }

        void StartTitleMusic() override
        {
        }

        void ToggleAllSounds() override
        {
        }
        void PauseSounds() override
        {
        }
        void UnpauseSounds() override
        {
        }

        void StopAll() override
        {
        }
        void StopCrowdSound() override
        {
        }
        void StopRideMusic() override
        {
        }
        void StopTitleMusic() override
        {
        }
        void StopVehicleSounds() override
        {
        }
    };

    std::unique_ptr<IAudioContext> CreateDummyAudioContext()
    {
        return std::make_unique<DummyAudioContext>();
    }
} // namespace OpenRCT2::Audio
