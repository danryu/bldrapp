**BEGIN PROMPT**

I am extending this GStreamer C++ application on macOS.
Right now the pipeline allows the user to select the camera before starting the pipeline.
This needs to be extended to allow dynamic switching of the camera device, while the pipeline is running.
I want you to refactor and extend the code in send_pipeline.cpp (and other files as necessary) to implement a **safe, reliable, non-hogging camera switching subsystem**, using the following architecture and requirements.
There is a static build of GStreamer at /Users/dan/code/gstreamer-build/install which can be updated as necessary to include support for more elements.

---

# 🔥 HIGH-LEVEL GOAL

Implement a **CameraSwitcher** class (or equivalent structure) with necessary changes to send_pipeline.cpp that allows me to dynamically switch between physical cameras **while the pipeline is running**, without tearing down the downstream pipeline, and **without hogging all cameras**.

* Only **one real camera** should ever be opened at a time.
* Switching must happen at runtime safely.
* Downstream elements should not need to renegotiate formats.
* The system must be compatible with macOS now (`avfvideosrc`) and be extendable to Windows later (probably `mfvideosrc`).

The solution **must not open all cameras** simultaneously.
Only the selected camera may be opened.

We are *not* implementing preview yet, but the design must leave space for adding preview later.

---

# 🔧 ARCHITECTURE REQUIREMENTS

### 1. **Use a single `input-selector` for the main pipeline**

The pipeline should look like:

```
[current camera source bin] → input-selector → videoconvert → sink  # (simplified)
```

Only one source bin is connected at any given time.

The `input-selector` exists only to prevent downstream renegotiation when switching.
It must **not** have multiple sources feeding it at the same time.

---

### 2. **Create and destroy a source bin dynamically**

When switching camera:

* Create a new bin containing:

  * `avfvideosrc` (macOS)
  * a queue or convert if appropriate
* Add this bin to the pipeline
* Link it to a new request pad on the `input-selector`
* Switch active pad
* Remove the old source bin entirely

This ensures **no camera hogging**, because only one source exists.

---

### 3. **No pre-creation of multiple camera sources**

This is crucial:

* Do **NOT** create a bin for every detected device.
* Do **NOT** attach multiple sources to the selector.
* Only the active camera source is instantiated.

This satisfies the requirement that the app must “not hog all camera sources”.

---

### 4. **Switch operation must be smooth and safe**

The switching steps should be:

1. Create new source bin for the new device
2. Add the bin to the pipeline
3. Link the new bin to the `input-selector` via a request pad
4. Sync its state with the pipeline
5. Set that pad as active
6. Remove the old pad and old source bin

If needed, use pad probes for blocking, but keep this minimal.

---

### 5. **Class Requirements**

Implement a C++ class like:

```
class CameraSwitcher {
public:
    bool start_pipeline();
    bool switch_camera(const std::string& device_id);
    void stop_pipeline();
private:
    GstElement *pipeline;
    GstElement *selector;
    GstElement *current_source;
};
```

Where:

* `device_id` is what `avfvideosrc` uses (e.g. unique ID or index)
* `current_source` is the only active camera
* The class handles cleanup on destruction

---

# 📌 IMPLEMENTATION DETAILS

### Source bin on macOS:

Use:

```
avfvideosrc device=[ID]
```

Wrap it in a bin with a queue:

```
[src] → [queue] → [ghostpad "src"]
```

### When switching:

* `gst_element_set_state(old_bin, NULL)`
* `gst_bin_remove(pipeline, old_bin)`
* `gst_element_release_request_pad(selector, old_pad)`

### When adding new:

* `gst_bin_add(pipeline, new_bin)`
* `gst_element_sync_state_with_parent(new_bin)`
* Request new selector pad
* Link
* Set selector active pad

---

# 🧩 LEAVE ROOM FOR FUTURE PREVIEW FEATURE

Write the code so that preview can be added later by:

* Creating temporary short-lived pipelines
  OR
* Creating temporary bins and immediately setting them to NULL afterwards

Make no assumptions that preview uses the same pipeline.

The main switching architecture must remain clean and independent.

---

# 🎯 WHAT TO GENERATE

Please generate:

1. A modified version of send_pipeline.cpp implementing this exact design.
2. A `CameraSwitcher` class with:

   * constructor
   * destructor
   * `start_pipeline()`
   * `switch_camera(device_id)`
   * `stop_pipeline()`
3. Complete pipeline setup using `input-selector`.
4. Modifications to main.qml to allow camera selection.
5. Comments explaining key steps.

Make it concise, but fully functional and correct for macOS using `avfvideosrc`.

Assume:

* GStreamer 1.26+
* C++17
* Basic error handling

Do not implement preview yet, but include clear comments where a preview system would integrate.

---

**END PROMPT**
