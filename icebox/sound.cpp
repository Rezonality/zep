#include "utils.h"
#include "utils/logger.h"

#include <map>

#include "jorvik.h"
#include "sound.h"

#include <m3rdparty/rtmidi/RtMidi.h>

#if defined(WIN32)
  #include <windows.h>
  #define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
  #include <unistd.h>
  #define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif
#undef ERROR

namespace Mgfx
{

struct GSound
{
    float music_volume = 1.0f;
    float effect_volume = 1.0f;
};

// File:
// One way load and compare
// /:music
// music_volume 1
// effect_volume 1
GSound sound;

void sound_init()
{
    //archive_bind(*jorvik.globalVars, "sound", "music_volume", sound.music_volume);
    // archive_bind_param(ar, sounds.effectVolume, "sound", "effects_volume");

    // Create an api map.
    std::map<int, std::string> apiMap;
    apiMap[RtMidi::MACOSX_CORE] = "OS-X CoreMIDI";
    apiMap[RtMidi::WINDOWS_MM] = "Windows MultiMedia";
    apiMap[RtMidi::UNIX_JACK] = "Jack Client";
    apiMap[RtMidi::LINUX_ALSA] = "Linux ALSA";
    apiMap[RtMidi::RTMIDI_DUMMY] = "RtMidi Dummy";

    std::vector<RtMidi::Api> apis;
    RtMidi ::getCompiledApi(apis);

    LOG(DEBUG) << "MIDI Compiled APIs:";
    for (unsigned int i = 0; i < apis.size(); i++)
    {
        LOG(DEBUG) << "  " << apiMap[apis[i]];
    }

    RtMidiIn* midiin = 0;
    RtMidiOut* midiout = 0;

    try
    {

        // RtMidiIn constructor ... exception possible
        midiin = new RtMidiIn();

        LOG(DEBUG) << "Current input API: " << apiMap[midiin->getCurrentApi()];

        // Check inputs.
        unsigned int nPorts = midiin->getPortCount();
        LOG(DEBUG) << "There are " << nPorts << " MIDI input sources available.";

        for (unsigned i = 0; i < nPorts; i++)
        {
            std::string portName = midiin->getPortName(i);
            LOG(DEBUG) << "  Input Port #" << i + 1 << ": " << portName;
        }

        // RtMidiOut constructor ... exception possible
        midiout = new RtMidiOut();

        LOG(INFO) << "Current output API: " << apiMap[midiout->getCurrentApi()];

        // Check outputs.
        nPorts = midiout->getPortCount();
        LOG(INFO) << "There are " << nPorts << " MIDI output ports available.";

        for (unsigned i = 0; i < nPorts; i++)
        {
            std::string portName = midiout->getPortName(i);
            LOG(INFO) << "  Output Port #" << i + 1 << ": " << portName;
        }
    }
    catch (RtMidiError& error)
    {
        LOG(ERROR) << error.getMessage();
    }

    /*
    std::vector<unsigned char> message;
    try
    {
        midiout->openPort(0);

        // Program change: 192, 5
        message.push_back(192);
        message.push_back(5);
        midiout->sendMessage(&message);

        SLEEP(10);

        message[0] = 0xF1;
        message[1] = 60;
        midiout->sendMessage(&message);

        // Control Change: 176, 7, 100 (volume)
        message[0] = 176;
        message[1] = 7;
        message.push_back(100);
        midiout->sendMessage(&message);

        // Note On: 144, 64, 90
        message[0] = 144;
        message[1] = 64;
        message[2] = 90;
        midiout->sendMessage(&message);

        SLEEP(250);

        // Note Off: 128, 64, 40
        message[0] = 128;
        message[1] = 64;
        message[2] = 40;
        midiout->sendMessage(&message);

        // Note On: 144, 64, 90
        message[0] = 144;
        message[1] = 64;
        message[2] = 90;
        midiout->sendMessage(&message);

        SLEEP(350);

        // Note Off: 128, 64, 40
        message[0] = 128;
        message[1] = 64;
        message[2] = 40;
        midiout->sendMessage(&message);

        SLEEP(10);
        // Control Change: 176, 7, 40
        message[0] = 176;
        message[1] = 7;
        message[2] = 40;
        midiout->sendMessage(&message);

        SLEEP(10);

        // Sysex: 240, 67, 4, 3, 2, 247
        message[0] = 240;
        message[1] = 67;
        message[2] = 4;
        message.push_back(3);
        message.push_back(2);
        message.push_back(247);
        midiout->sendMessage(&message);
    }
    catch (std::exception&)
    {
    }
    */

    // Clean up
    delete midiout;
    delete midiin;
}

void sound_destroy()
{
}

} // namespace Mgfx