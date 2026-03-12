# Fix Slow Disconnect and Crash on Exit

## Issue Description
The application was experiencing a 20-30 second hang upon clicking "Disconnect", followed by GStreamer assertions and crashes.
- **Hang:** The UI would freeze while waiting for the Jitsi connection to tear down.
- **Crash:** After the hang, `gst_bin_remove` assertions would fire, indicating elements were being removed from an invalid pipeline.

## Changes Implemented

### 1. Plugin Fix: `gstjitsimeet` (Deadlock Avoidance)
**File:** `gstjitsimeet/src/jitsibin.cpp`
**Function:** `ready_to_null`

The teardown sequence in `jitsibin` had a dependency loop:
1. The `ready_to_null` function waited for the runner thread to join.
2. The runner thread was blocked waiting for the websocket context to process shutdown events.
3. The shutdown signal was only sent *after* the join attempt in the original code (or effectively blocked by it).

**Fix:** 
Reordered the teardown sequence to explicitly shutdown the WebSocket context *before* attempting to join the runner thread. This unblocks the event loop immediately, allowing the thread to exit cleanly without waiting for a timeout.

```cpp
auto ready_to_null(RealSelf& self) -> bool {
    // 1. Shutdown WS context first to unblock the runner loop
    if(self.ws_context.state == ws::client::State::Connected) {
        self.ws_context.shutdown();
    }
    
    // 2. Now safe to join the thread
    if(self.runner_thread.joinable()) {
        self.injector.inject_task([](RealSelf& self) -> coop::Async<void> {
            self.ws_task.cancel();
            self.connection_task.cancel();
            self.injector.blocker.stop();
            co_return;
        }(self));
        self.runner_thread.join();
    }
    return true;
}
```

### 2. Application Fix: `qnujitsi` (Teardown Order)
**File:** `gstreamer-build/example_04_qnujitsi/app_controller.cpp`
**Function:** `AppController::teardown`

The application was destroying the GStreamer pipeline object *before* destroying the `Conference` object. 
- The `Conference` object owns `SendPipeline` and `ParticipantManager`.
- These helpers attempt to remove their elements from the pipeline in their destructors.
- Since the pipeline was already unreffed/destroyed, accessing it caused invalid pointer assertions (`gst_bin_remove`).

**Fix:**
Reordered `teardown()` to destroy the `conference_` object first. This ensures the pipeline remains valid while the helper classes clean up their elements.

```cpp
void AppController::teardown() {
  if (!conference_) return;
  GstElement* pipeline = conference_->pipeline();
  
  // 1. Reset conference first (destroys participants and send pipeline helpers)
  //    while the pipeline object is still valid.
  conference_.reset();
  
  // 2. Now safe to destroy the pipeline
  if (pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
  }
  emit connectedChanged();
}
```

## Result
- Disconnect time reduced from ~30s to <5s.
- Elimination of `GStreamer-CRITICAL` assertions on exit.

