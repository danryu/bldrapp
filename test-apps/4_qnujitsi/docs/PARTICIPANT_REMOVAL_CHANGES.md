# Participant Removal Changes in qnujitsi

## Overview

This document details the changes made to `example_04_qnujitsi` to correctly handle participant removal and cleanup of GStreamer pipeline resources and QML video slots.

## Context

Previously, when a participant left the Jitsi conference:
1. The `jitsibin` element was not emitting a `pad-removed` signal (likely due to underlying implementation details in `gst-meet` or `jitsibin`).
2. The `participant-left` signal was firing, but there was no logic to map this event to the specific GStreamer resources associated with that participant.
3. Consequently, the video pipeline elements (`h264parse`, `vtdec`) remained active, and the `GstGLQt6VideoItem` in the UI continued to display the last received frame (frozen).

## Changes Implemented

### 1. Tracking Participant Pads

We introduced a mapping to associate participant IDs with their GStreamer pads.

*   **File**: `participant_manager.h`, `participant_manager.cpp`
*   **Mechanism**: A `std::map<std::string, std::vector<GstPad*>> participantPads_` was added.
*   **Logic**: In `handlePadAdded`, we now parse the participant ID from the pad name (format: `id_codec_ssrc`) and store the pad in this map.

### 2. Handling `participant-left` Signal

Since `pad-removed` is unreliable/unimplemented, we now rely on `participant-left` to trigger cleanup.

*   **Handler**: `handleParticipantLeft`
*   **Logic**:
    1. Receives the `participant_id`.
    2. Look up all associated pads in `participantPads_`.
    3. Calls `teardownPad` for each found pad.
    4. Removes the participant entry from the map.

### 3. Pipeline Teardown Logic (`teardownPad`)

A centralized `teardownPad` helper function was implemented to safely destroy the receive chain.

*   **Process**:
    1. Identifies the active session (elements and slot index) for the given pad.
    2. **Unlinks** the decoder from the slot's permanent `videoconvert` element.
    3. Sets the session-specific elements (`h264parse`, `vtdec`) to `GST_STATE_NULL`.
    4. **Removes** these elements from the pipeline bin.
    5. Marks the video slot as free in `inUse_`.

### 4. UI Cleanup (Fixing Frozen Video)

To solve the "frozen frame" issue, we must hide the QML video item when the slot is freed.

*   **Mechanism**: `QMetaObject::invokeMethod` with lambdas.
*   **Logic**:
    *   When a slot is **freed** (in `teardownPad`): We invoke `setVisible(false)` on the corresponding `QQuickItem` (the `videoItem`) on the main thread.
    *   When a slot is **reserved** (in `handlePadAdded`): We invoke `setVisible(true)` on the item.
    *   **Thread Safety**: Cross-thread updates to QML/GUI objects are handled safely using `Qt::QueuedConnection`.

### 5. Removed `pad-removed` Handler

The unused and confusing `pad-removed` signal handler was removed entirely to simplify the code and rely solely on the working `participant-left` signal.

## Summary of Files Modified

*   `gstreamer-build/example_04_qnujitsi/participant_manager.h`
*   `gstreamer-build/example_04_qnujitsi/participant_manager.cpp`

