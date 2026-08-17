#include "LeirEngine/Audio/AudioEngine.h"
#include "LeirEngine/Audio/AudioClip.h"

#include "SoLoudBackend.h"

#include "LeirEngine/Core/Log.h"

#include <unordered_map>

namespace Leir {

namespace {
std::unordered_map<std::string, ClipId>& ClipCache()
{
    static std::unordered_map<std::string, ClipId> cache;
    return cache;
}
}

AudioEngine& AudioEngine::GetInstance()
{
    static AudioEngine instance;
    return instance;
}

AudioEngine::~AudioEngine()
{
    Shutdown();
}

void AudioEngine::Init()
{
    if (m_Initialized)
        return;

    m_Backend = std::make_unique<SoLoudBackend>();
    if (!m_Backend->Init()) {
        XConsole::PrintError("[Audio] AudioEngine: backend failed to init");
        m_Backend.reset();
        return;
    }
    m_Initialized = true;
}

void AudioEngine::Shutdown()
{
    if (m_Backend) {
        m_Backend->Shutdown();
        m_Backend.reset();
    }
    m_Initialized = false;
}

void AudioEngine::Update(float deltaTime)
{
    if (m_Backend)
        m_Backend->Update(deltaTime);
}

void AudioEngine::WakeUp()
{
    if (m_Backend)
        m_Backend->WakeUp();
}

ClipId AudioEngine::GetOrLoadClip(const std::string& path)
{
    if (!m_Initialized)
        return 0;
    if (!m_Backend)
        return 0;

    auto& cache = ClipCache();
    auto it = cache.find(path);
    if (it != cache.end())
        return it->second;

    ClipId id = m_Backend->LoadClip(path);
    if (id != 0)
        cache.emplace(path, id);
    return id;
}

std::shared_ptr<AudioClip> AudioEngine::GetClipAsset(const std::string& path)
{
    ClipId id = GetOrLoadClip(path);
    if (id == 0)
        return nullptr;
    return AudioClip::Create(path, m_Backend->GetClipDuration(id), id);
}

void AudioEngine::UnloadAllClips()
{
    if (!m_Backend)
        return;
    for (auto& [path, id] : ClipCache())
        m_Backend->UnloadClip(id);
    ClipCache().clear();
}

} // namespace Leir