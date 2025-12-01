# Fix: Camera Switch/Failure on Unmute (macOS)

## Problem Description

When using the camera mute toggle ("On/Off" button), unmuting the camera would often result in:
1.  **Wrong camera activating:** The camera would switch to a different device (e.g., from an external webcam to the built-in FaceTime camera).
2.  **Failure to start:** The camera might fail to resume entirely.

This behavior matches the instability observed in the Camera Selection bug (see [fix-camera-selection.md](./fix-camera-selection.md)).

## Root Cause

The issue stems from the same underlying behavior in the macOS AVFoundation framework: **Device handles become stale after use.**

When the camera is muted in `qnujitsi`, we stop the source element (`GST_STATE_NULL`) to release the hardware resource and turn off the LED. This invalidates the underlying AVFoundation device handle.

Previously, unmuting simply attempted to restart (`GST_STATE_PLAYING`) the existing `avfvideosrc` element. Because the element was holding a stale handle, the OS either rejected the request or defaulted to a different device index, causing the wrong camera to open.

## Solution: "Replace-on-Unmute" Strategy

We applied a similar logic to the camera selection fix: **Always use a fresh GstDevice object when opening a camera.**

### Implementation Details

The unmute flow was modified to perform a full "hot-swap" of the video source element:

1.  **Fresh Enumeration (AppController):**
    *   When `setVideoMuted(false)` is called, `AppController` immediately re-enumerates cameras via `CameraManager`.
    *   It locates the currently selected camera by its **display name** (which remains stable, unlike device indices or handles).
    *   It retrieves a fresh `GstDevice*` object for that camera.

2.  **Element Replacement (SendPipeline):**
    *   `SendPipeline::setVideoMuted` was updated to accept this fresh `GstDevice*`.
    *   Instead of just changing the state of the existing source, it now:
        1.  Removes the old `videosrc` element from the pipeline.
        2.  Creates a **new** `avfvideosrc` element using the fresh `GstDevice*`.
        3.  Links the new element to the existing pipeline (`videoconvert` -> ...).
        4.  Syncs the new element's state with the parent pipeline.

### Code Changes

*   **`AppController::setVideoMuted`**: Added logic to re-enumerate cameras and find the fresh device handle before calling the conference unmute method.
*   **`Conference::setVideoMuted`**: Updated signature to pass the optional `GstDevice*` pointer.
*   **`SendPipeline::setVideoMuted`**: Implemented the hot-swap logic. It now checks if a new device is provided and, if so, replaces the source element instead of just restarting it.

### Result

Unmuting now reliably restores the exact same camera that was in use before muting, regardless of how many times the action is performed or what other devices are connected.

