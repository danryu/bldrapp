Summary of Changes
1. send_pipeline.h
Added volume_ element pointer for audio muting
Added videoMuted_ and audioMuted_ state flags
Added public mute methods: setVideoMuted(), setAudioMuted(), isVideoMuted(), isAudioMuted()
2. send_pipeline.cpp
Created volume element in createElements()
Added volume to bin and linked into audio path: osxaudiosrc → audioconvert → volume → opusenc
Implemented setVideoMuted(): uses gst_element_set_locked_state() + state transition to safely stop/start camera
Implemented setAudioMuted(): uses GStreamer volume element's mute property
Updated destructor and syncStateWithParent() to handle volume element
3. conference.h / conference.cpp
Added mute forwarding methods that delegate to SendPipeline
4. app_controller.h / app_controller.cpp
Added Q_PROPERTY bindings for videoMuted and audioMuted
Added signals videoMutedChanged() and audioMutedChanged()
Implemented getter/setter methods that delegate to Conference
5. main.qml
Added two toggle buttons in the toolbar:
📷 On/Off - toggles camera mute
🎤 On/Off - toggles microphone mute
Buttons are enabled only when connected
How It Works
Camera Mute:
Uses gst_element_set_locked_state(TRUE) to prevent parent state changes from affecting the camera
Sets camera to GST_STATE_NULL to fully stop it (LED goes off)
On unmute, unlocks state and syncs with parent to resume
Microphone Mute:
Uses GStreamer's volume element with its native mute property
Clean and efficient—no pipeline restructuring needed