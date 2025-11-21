## **📌 Cursor Prompt: Build Robust Dynamic Camera Switching in GStreamer (C++)**

You are improving an existing C++ GStreamer pipeline that currently uses a static `avfvideosrc`.
Rewrite+extend the code to implement **robust, zero-glitch dynamic camera switching** using the following design constraints.

### **PRIMARY GOAL**

Update the file send_pipeline.cpp (and other files as necessary) to make a pipeline where:

* The *camera source* can be switched at runtime safely
* Switching uses **pad-blocked probes** to avoid glitches, green frames, renegotiation bursts, or race conditions
* The design isolates the camera from the rest of the pipeline via an **intermediate ghost pad** and a **replaceable source bin**
* Future support for local "preview" is possible, but not required now

---

## **📌 REQUIRED ARCHITECTURE**

### **1. Device Auto-Detection**

Implement a helper class:

```
class CameraEnumerator {
public:
    struct CameraInfo { std::string name; std::string gstDeviceString; };
    std::vector<CameraInfo> listCameras();
};
```

It must:

* Use **GstDeviceMonitor**
* Filter for `Video/Source` class
* For macOS, return devices whose device provider is “avfvideosrc”
* Provide the necessary **device-string** that can be set via `device=` property

---

### **2. A Replaceable Source Bin**

Create a helper:

```
GstElement* create_camera_bin(const std::string& device_string);
```

This bin should:

* Contain `avfvideosrc` (macOS) configured with `device=device_string`
* Contain optional `identity` (for debug)
* End with a `capsfilter` (720p fixed caps recommended)
* Expose a **single src ghost pad** named `"src"`

---

### **3. Pipeline Layout**

```
camera_bin ---> queue ---> converter ---> encoder ---> whatever...
```

But **camera_bin must be replaceable** without touching the rest of the pipeline.

Require:

* A **dedicated “camera entry queue” element**, which will be blocked during switch
* Dynamic pad linking only between:

  * camera_bin.src ↔ entry_queue.sink

This queue is the switching boundary.

---

### **4. Switching Mechanism (Pad-Probe Blocking)**

Create a helper:

```
void switch_camera(GstElement* pipeline,
                   GstElement** current_camera_bin_ptr,
                   const std::string& new_device_string);
```

Inside the function:

1. Get the sink pad of the **entry queue**
2. Install a **blocking pad probe** of type `GST_PAD_PROBE_TYPE_BLOCK`
3. In the probe callback:

   * Unlink old camera_bin → queue
   * Remove old camera_bin from pipeline
   * Create new camera_bin with `create_camera_bin(...)`
   * Add + link the new camera_bin → queue
   * Sync state with parent
4. Remove the probe
5. Return

This ensures:

* Perfect switching
* No broken timestamps
* No green flash
* No renegotiation storms
* No “not-linked” or “stream-start” warnings

---

### **5. Thread Safety + Reentrancy**

* Switching must only be triggered from the **main GStreamer bus thread** or via `g_main_context_invoke()`
* No GLib blocking operations in pad-probe callback
* All element state changes must be done outside pad-probe (use idle handler), except unlink/link which must occur inside the blocked section

---


### For `/complete`

* Provide **full compilable code**, including:

  * `#include` directives
  * main()
  * helper classes
  * comments for each block
  * no unrelated text before or after code
* Code should compile on macOS with local installed GStreamer

### For `/edit`

* Apply changes *surgically*
* Only modify what is needed for:

  * device monitor
  * source-bin creation
  * switching logic
* Maintain project structure
* Do not wrap everything in a different abstraction unless explicitly asked
* Keep functions short, readable, and safe

---

## **📌 LEAVE OPEN FOR FUTURE PREVIEW MODE**

* Add `// TODO: Support optional preview tee later` in the camera bin
* Do not add a tee or queue now
* Make sure the bin can be extended without breaking the switching logic

---

## **📌 Summary of What You Must Produce**

The result of this prompt should include:

* Full C++ implementation of:

  * CameraEnumerator (GstDeviceMonitor)
  * create_camera_bin()
  * switch_camera() with pad-probe blocking
  * Modified send_pipeline.cpp incorporating the above strategy
  * Modified main.qml to allow camera selection

* Code that is tidy, well-commented, and follows the architecture above

