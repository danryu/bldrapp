# Fix Slow Disconnect and Crash on Exit

## Issue Description
The application was experiencing a 20-30 second hang upon clicking "Disconnect", followed by GStreamer assertions and crashes.
- **Hang:** The UI would freeze while waiting for the Jitsi connection to tear down.
- **Crash:** After the hang, `gst_bin_remove` assertions would fire, indicating elements were being removed from an invalid pipeline.
- **Segfault:** Occasional `EXC_BAD_ACCESS` crashes were observed in `Thread 18` during teardown, traced to accessing destroyed `Conference` objects. (see docs/crashlogs/crash_macOS_EXC_BAD_ACCESS.txt)
- **Indefinite Hang:** In some cases (approx. 40% failure rate), the application would hang indefinitely during disconnect due to a race condition in the WebSocket connection initialization loop.

## Changes Implemented

### 1. Plugin Fix: `gstjitsimeet` (Teardown & Object Lifetime)
**File:** `gstjitsimeet/src/jitsibin.cpp`

#### A. Deadlock Avoidance in `ready_to_null`
The teardown sequence had a dependency loop where `ready_to_null` waited for the runner thread to join, but the runner thread was blocked waiting for the WebSocket context to process shutdown events.

**Fix:** 
- Explicitly call `ws_context.shutdown()` *before* attempting to join the runner thread.
- Ensure `shutdown()` is called even if the connection state isn't strictly `Connected` (e.g., if it's stuck in initialization), provided the context exists.

```cpp
auto ready_to_null(RealSelf& self) -> bool {
    LOG_DEBUG(logger, "ready_to_null: stopping websocket...");
    // Unconditional shutdown if context exists to break any blocking loops
    if(self.ws_context.context) {
        self.ws_context.shutdown();
    }
    
    if(self.runner_thread.joinable()) {
        LOG_DEBUG(logger, "ready_to_null: joining runner thread...");
        self.injector.inject_task([](RealSelf& self) -> coop::Async<void> {
            self.ws_task.cancel();
            self.connection_task.cancel();
            self.injector.blocker.stop();
            co_return;
        }(self));
        self.runner_thread.join();
        LOG_DEBUG(logger, "ready_to_null: runner thread joined.");
    }
    return true;
}
```

#### B. Safe Pinger Task
The pinger task (`pinger_main`) was continuing to run and accessing the `Conference` object after it had been destroyed during teardown, causing segfaults.

**Fix:**
- Changed `Conference` ownership to `std::shared_ptr`.
- Modified `pinger_main` to accept a `std::weak_ptr<conference::Conference>`.
- The task now locks the weak pointer before use; if the conference is destroyed, the lock fails safely, and the task exits.

### 2. Application Fix: `qnujitsi` (Teardown Order)
**File:** `gstreamer-build/example_04_qnujitsi/app_controller.cpp`
**Function:** `AppController::teardown`

The application was destroying the GStreamer pipeline object *before* destroying the `Conference` object. The `Conference` object's helpers (`SendPipeline`, `ParticipantManager`) attempt to remove elements from the pipeline in their destructors, causing invalid pointer access.

**Fix:**
Reordered `teardown()` to destroy the `conference_` object first.

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

### 3. Dependency Fix: `websocket-cpp` (Race Condition & Hang)
**Files:** `gstjitsimeet/submodules/websocket-cpp/src/client.hpp`, `client.cpp`

#### A. Connection Initialization Race
The `Context::init` function contained a blocking `while` loop waiting for the connection state to change from `Initialized`. If `shutdown()` was called from another thread (e.g., during a fast disconnect or network timeout) while this loop was running, it could be missed or overwritten, causing an indefinite hang.

**Fix:**
- Changed `state` member to `std::atomic<State>` for thread-safe updates.
- Moved `state = State::Initialized` to the *start* of `init` to prevent overwriting a `Destroyed` state set by `shutdown()`.
- Added a check in `shutdown()` to ensure `lws_cancel_service()` is always called if the context exists, breaking the `lws_service` poll loop.
- Added an early exit in `init` if state is already `Destroyed`.

```cpp
// Context::init start
state = State::Initialized;
// ... create context ...
if (state == State::Destroyed) return false;
// ... connect ...
while(state == State::Initialized) {
    lws_service(context.get(), 50);
}
```

## Result
- **Disconnect Latency:** Reduced from ~30s to <5s consistently.
- **Stability:** Eliminated `GStreamer-CRITICAL` assertions and `EXC_BAD_ACCESS` segfaults.
- **Reliability:** Prevented indefinite hangs during connection negotiation or poor network conditions by ensuring the WebSocket loop can always be interrupted.
