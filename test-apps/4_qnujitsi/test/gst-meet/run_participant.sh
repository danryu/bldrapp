#!/bin/bash
set -e

# Configuration from environment variables with defaults
SERVER=${JITSI_SERVER:-"localhost:30443"}
ROOM=${JITSI_ROOM:-"testroom"}
PARTICIPANT_NAME=${PARTICIPANT_NAME:-"gst-meet-bot"}
VIDEO_PATTERN=${VIDEO_PATTERN:-"smpte"}
AUDIO_WAVE=${AUDIO_WAVE:-"sine"}
AUDIO_FREQ=${AUDIO_FREQ:-"440"}
VIDEO_WIDTH=${VIDEO_WIDTH:-"640"}
VIDEO_HEIGHT=${VIDEO_HEIGHT:-"480"}
VIDEO_FPS=${VIDEO_FPS:-"30"}

echo "========================================="
echo "gst-meet Participant Simulator"
echo "========================================="
echo "Server:      $SERVER"
echo "Room:        $ROOM"
echo "Name:        $PARTICIPANT_NAME"
echo "Video:       ${VIDEO_PATTERN} (${VIDEO_WIDTH}x${VIDEO_HEIGHT}@${VIDEO_FPS}fps)"
echo "Audio:       ${AUDIO_WAVE} wave at ${AUDIO_FREQ}Hz"
echo "========================================="

# Construct send pipeline (combining audio and video sources)
# gst-meet 0.5.0+ expects a complete pipeline with elements named 'audio' and 'video'
SEND_PIPELINE="videotestsrc is-live=true ! queue ! videoscale ! video/x-raw,width=640,height=360 ! videoconvert ! x264enc tune=zerolatency speed-preset=ultrafast bitrate=1000 ! h264parse ! queue name=video audiotestsrc wave=${AUDIO_WAVE} freq=${AUDIO_FREQ} is-live=true ! audio/x-raw,rate=48000,channels=1 ! opusenc bitrate=64000 ! queue name=audio"

# Construct WebSocket URL from server
# Using wss:// for secure WebSocket (HTTPS deployment)
WS_URL="wss://${SERVER}/xmpp-websocket"

# Launch gst-meet with TLS verification disabled for self-signed certs
echo "Launching gst-meet..."
export GST_DEBUG=3
exec gst-meet \
    --web-socket-url="${WS_URL}" \
    --room-name="${ROOM}" \
    --nick="${PARTICIPANT_NAME}" \
    --video-codec="h264" \
    --send-pipeline="${SEND_PIPELINE}" \
    --tls-insecure \
    --verbose
