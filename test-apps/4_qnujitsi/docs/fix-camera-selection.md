# Fix: Camera Selection Instability on macOS

## Problem Description

Camera selection became unpredictable after 2-3 Connect/Disconnect cycles. The selected camera in the dropdown menu would not match the actual camera that opened in the conference.

### Symptoms

- Initial connection: Camera selection worked correctly
- After changing camera selection and reconnecting: Correct camera activated
- After 3rd+ connection: **Wrong camera activated despite dropdown showing correct selection**
- The issue persisted without any user interaction with the dropdown

### Example of the Bug

```
User selects: "FaceTime HD Camera"
1st connect: Opens "FaceTime HD Camera" 
Disconnect
2nd connect: Opens "FaceTime HD Camera" 
Disconnect
3rd connect: Opens "FHD Camera"  (WRONG!)
```

## Root Cause Analysis

### Investigation Steps

1. **Initial hypothesis:** Device-index values from `GstDeviceMonitor` were non-deterministic
   - **Finding:** Device indices were being read correctly from enumeration
   - **Result:** Not the issue

2. **Second hypothesis:** Using integer device-index property on `avfvideosrc` elements
   - **Finding:** AVFoundation device-index mappings are **volatile** - they change after camera usage
   - **Evidence:**
     ```
     2nd connect: device-index=1 ’ "FaceTime HD Camera" 
     3rd connect: device-index=1 ’ "FHD Camera" 
     ```
   - **Result:** Partial issue - led to solution attempt #1

3. **Solution attempt #1:** Store and use `GstDevice` objects instead of device-index integers
   - **Implementation:** Modified `CameraDevice` to hold `GstDevice*` with proper ref-counting
   - **Finding:** Even `GstDevice` objects become **stale** after camera usage on macOS!
   - **Evidence:**
     ```
     2nd connect: GstDevice A ’ "FaceTime HD Camera" 
     3rd connect: Same GstDevice A ’ "FHD Camera" 
     ```
   - **Result:** Still broken - GstDevice's internal AVFoundation reference becomes invalid

### Core Issue: macOS AVFoundation Device Handle Volatility

**The fundamental problem:** On macOS, AVFoundation device handles (whether accessed via device-index integer OR GstDevice object) become invalid/stale after:
1. Opening a camera device
2. Using it in a pipeline
3. Destroying the pipeline and releasing the device
4. Attempting to open the device again

The OS-level AVFoundation framework **remaps device identifiers** after device release, breaking both:
- Direct `device-index` integer property approach
- `GstDevice` object approach (which internally uses AVFoundation handles)

## Solution

### Approach: Fresh Enumeration + Name-Based Selection

Since device handles become stale, we must:
1. **Re-enumerate devices before each connection** to get fresh handles
2. **Select cameras by display name** (which is stable) rather than by stale references

### Implementation

#### 1. Store GstDevice Objects ([camera_manager.h](../camera_manager.h))

```cpp
struct CameraDevice {
    GstDevice* device;         // GstDevice object (ref-counted)
    std::string displayName;   // Human-readable name (STABLE identifier)

    CameraDevice(GstDevice* dev, const std::string& dispName);
    ~CameraDevice();
    // Move semantics for proper ref-counting
};
```

**Key points:**
- `GstDevice*` provides proper AVFoundation device configuration
- `displayName` is the **stable** identifier we use for matching
- Proper ref-counting via constructor/destructor
- Move semantics prevent double-unref

#### 2. Create avfvideosrc from GstDevice ([send_pipeline.cpp](../send_pipeline.cpp))

```cpp
bool SendPipeline::createElements(bool withLocalPreview, GstDevice* cameraDevice) {
  if (cameraDevice) {
    // Create avfvideosrc from GstDevice (bypasses unstable device-index)
    videosrc_ = gst_device_create_element(cameraDevice, nullptr);
  }
}
```

**Why this matters:** `gst_device_create_element()` uses GstDevice's internal configuration rather than the volatile `device-index` property.

#### 3. Re-enumerate Before Each Connect ([app_controller.cpp](../app_controller.cpp))

```cpp
bool AppController::connectToConference(...) {
  // Re-enumerate cameras to get fresh GstDevice objects
  // This is necessary on macOS where AVFoundation device handles become stale after use
  QString selectedCameraName;
  if (cameraManager_) {
    // Save current selection by NAME
    const CameraDevice* currentSelection = cameraManager_->selectedCamera();
    if (currentSelection) {
      selectedCameraName = QString::fromStdString(currentSelection->displayName);
    }

    // Get fresh GstDevice objects from AVFoundation
    cameraManager_->enumerateCameras();

    // Re-select the same camera by matching display name
    if (!selectedCameraName.isEmpty()) {
      for (int i = 0; i < cameraManager_->cameraNames().size(); ++i) {
        if (cameraManager_->cameraNames()[i] == selectedCameraName) {
          cameraManager_->setCurrentCameraIndex(i);
          break;
        }
      }
    }
  }

  // Get fresh GstDevice for connection
  GstDevice* cameraDevice = nullptr;
  if (cameraManager_) {
    const CameraDevice* selectedCamera = cameraManager_->selectedCamera();
    if (selectedCamera) {
      cameraDevice = selectedCamera->device;  // Fresh device handle
    }
  }
}
```

#### 4. Sort by Display Name for Stability ([camera_manager.cpp](../camera_manager.cpp))

```cpp
// Sort cameras by display name to ensure stable order across enumerations
std::sort(cameras_.begin(), cameras_.end(), [](const CameraDevice& a, const CameraDevice& b) {
    return a.displayName < b.displayName;
});
```

**Why:** Even with fresh enumeration, GstDeviceMonitor may return devices in different orders. Sorting ensures consistent array indices.

## Why This Solution Works

### The Chain of Stability

1. **Display name is stable** - "FaceTime HD Camera" doesn't change between enumerations
2. **Fresh GstDevice objects** - Each connection gets a current AVFoundation handle
3. **Name-based re-selection** - User's choice is preserved across re-enumerations
4. **Deterministic ordering** - Alphabetical sort ensures consistent array positions

### Data Flow

```
User selects camera from dropdown
  “
Store display name: "FaceTime HD Camera"
  “
On Connect:
  Re-enumerate ’ [GstDevice_new_A, GstDevice_new_B]
  “
  Find "FaceTime HD Camera" by name ’ index 1
  “
  Set currentCameraIndex = 1
  “
  Get fresh GstDevice from cameras_[1]
  “
  Create avfvideosrc from fresh GstDevice
  “
   Correct camera opens
```

## Key Learnings

### Device-Index Volatility

The `device-index` property on `avfvideosrc` is **not stable** across camera usage cycles on macOS:

```
Initial state:    device-index=0 ’ "FHD Camera"
                  device-index=1 ’ "FaceTime HD Camera"

After using and releasing FaceTime camera:
                  device-index=0 ’ "FaceTime HD Camera"  (CHANGED!)
                  device-index=1 ’ "FHD Camera"          (CHANGED!)
```

### GstDevice Staleness

Even `GstDevice` objects returned by `GstDeviceMonitor` become stale after the underlying hardware device is used and released. The `GstDevice` object itself remains valid (doesn't crash), but its internal AVFoundation device reference is no longer correct.

### Why Audio Selection Worked

The `osxaudiosrc` element uses CoreAudio device IDs (not sequential indices), which appear to be more stable than AVFoundation video device indices. Additionally, audio devices may be less susceptible to this remapping behavior.

## Testing

Verified the fix works across multiple scenarios:

1. **Initial selection:** Camera selection works
2. **Reconnection:** Same camera opens consistently
3. **Device change:** Switching cameras works correctly
4. **Multiple cycles:** 10+ connect/disconnect cycles maintain correct selection
5. **Device-index changes visible but harmless:** Logs show device-index values changing (0”1), but **correct camera always opens** because we're using name-based selection with fresh GstDevice objects

## Files Modified

- `camera_manager.h` - Changed `CameraDevice` to store `GstDevice*` instead of device-index string
- `camera_manager.cpp` - Store GstDevice objects, sort by name, proper ref-counting
- `send_pipeline.h` - Accept `GstDevice*` parameter instead of device-index string
- `send_pipeline.cpp` - Create avfvideosrc from GstDevice using `gst_device_create_element()`
- `conference.h/cpp` - Pass `GstDevice*` through the call chain
- `app_controller.cpp` - Re-enumerate cameras before each connect, re-select by name
