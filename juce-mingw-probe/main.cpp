#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

// Minimal probe: compile + link juce_audio_devices with JUCE_ASIO on MinGW64.
int main()
{
    return JUCE_ASIO;
}
