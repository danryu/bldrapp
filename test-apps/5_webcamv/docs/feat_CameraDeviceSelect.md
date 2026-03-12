# Feature: Camera Device Selection

This document describes the technical implementation of camera device selection in the application, including the mechanism for enumerating devices and ensuring robust selection stability on macOS.

## Overview

The application uses GStreamer's `GstDeviceMonitor` API to discover available video source devices (webcams). A custom `CameraManager` class exposes this list to the QML UI and handles the creation of GStreamer source elements for the selected device.

## Architecture

### 1. CameraManager (C++ Backend)

The `CameraManager` class serves as the bridge between GStreamer's device monitoring and the Qt/QML UI.

-   **Responsibility**: Enumerates devices, maintains the list of available cameras, and tracks the currently selected camera index.
-   **Data Structure**: Stores a list of `CameraDevice` structs, each containing:
    -   `std::string displayName`: The human-readable name of the camera (used for UI and stable identification).
    -   `GstDevice* device`: A reference-counted pointer to the GStreamer device object.

### 2. Device Enumeration (`enumerateCameras`)

The enumeration process is designed to be robust against device handle invalidation, particularly on macOS.

1.  **Preserve Selection**: Before clearing the current list, the display name of the currently selected camera is saved.
2.  **Monitor Creation**: A `GstDeviceMonitor` is created and configured to filter for `Video/Source` devices with `video/x-raw` capabilities.
3.  **Discovery**: The monitor queries the system for connected devices.
4.  **List Population**: `GstDevice` objects are collected and wrapped in `CameraDevice` structs.
5.  **Sorting**: The list is sorted alphabetically by display name to ensure a deterministic order in the UI.
6.  **Restoration**: The code attempts to find the previously selected camera by matching its display name in the new list.
    -   If found, the selection index is updated to point to the new entry.
    -   If not found (e.g., device disconnected), the selection defaults to the first available camera or an empty state.

### 3. macOS Device Handle Stability

A critical challenge on macOS is that AVFoundation device handles (underlying `avfvideosrc`) become "stale" after they are used in a pipeline and that pipeline is stopped. Reusing an old `GstDevice` object or `device-index` to create a new element often results in opening the wrong camera or failing to open the device.

**The Solution:**
-   **Fresh Enumeration**: The application calls `enumerateCameras()` immediately before creating a new pipeline (in `startPipeline()`).
-   **Fresh Handles**: This ensures that `createCurrentCameraElement()` always uses a newly obtained `GstDevice` handle from the system.
-   **Stable Identification**: Since the list is re-generated, we rely on the **display name** (which remains constant) to persist the user's selection across these refreshes.

### 4. Pipeline Integration

-   **Pipeline Creation**: When the pipeline is started, `CameraManager::createCurrentCameraElement()` is called.
-   **Element Factory**: It uses `gst_device_create_element()` with the fresh `GstDevice` handle. This allows GStreamer to configure the source element (e.g., `avfvideosrc` on macOS, `ksvideosrc` on Windows, `v4l2src` on Linux) with the specific device parameters without relying on fragile integer indices.

### 5. UI Integration (QML)

-   The `CameraManager` is exposed as a context property to QML.
-   A `ComboBox` is populated with `cameraManager.cameraNames`.
-   When the user changes the selection, a signal triggers the application to stop the current pipeline and restart it with the new source.

## Key Files

-   `camera_manager.h` / `.cpp`: Core logic for device enumeration and management.
-   `main.cpp`: Application controller that manages the GStreamer pipeline and coordinates with `CameraManager`.
-   `main.qml`: UI layer presenting the camera list.

