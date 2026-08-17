#include "LeirEngine/Audio/AudioClip.h"

#include "LeirEngine/Audio/AudioEngine.h"

namespace Leir {

std::shared_ptr<AudioClip> AudioClip::Load(const std::string& path)
{
    return AudioEngine::GetInstance().GetClipAsset(path);
}

std::shared_ptr<AudioClip> AudioClip::Create(const std::string& path, float duration, ClipId clipId)
{
    if (clipId == 0)
        return nullptr;
    std::shared_ptr<AudioClip> clip(new AudioClip());
    clip->m_Path = path;
    clip->m_Duration = duration;
    clip->m_ClipId = clipId;
    return clip;
}

} // namespace Leir