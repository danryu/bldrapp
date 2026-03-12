# Fix Crash on Disconnect

## Issue Description

The application experienced multiple crash types when clicking "Disconnect":

1. **Hang (spinning beach ball)** - UI frozen indefinitely
2. **SIGABRT** - `std::mutex::lock()` threw exception in `ParticipantManager::handleMuteStateChanged`
3. **SIGSEGV** - Null pointer dereference in `conference::Conference::send_iq()` or Metal/GL access violations

All crashes were caused by **race conditions during teardown** where background threads continued accessing objects that were being destroyed.

## Root Cause Analysis

### Crash 1: SIGABRT in ParticipantManager

The jitsibin runner thread emitted `mute-state-changed` signal while `ParticipantManager` was being destroyed:

```
Thread 15 (crashed):
  std::__1::mutex::lock()
  ParticipantManager::handleMuteStateChanged()
  ConferenceCallbacks::on_mute_state_changed()
  conference::handle_received (.resume)
  coop::Runner::run()

Thread 0 (main, blocked):
  gst_element_set_state(NULL)
  AppController::teardown()
```

The mutex was in an invalid/destroyed state when the signal handler tried to acquire it.

### Crash 2: SIGSEGV in send_iq (pinger)

The `pinger_main` coroutine held a reference to `conference`, a local `unique_ptr` in `connect_to_conference`. When the connection task was cancelled, the conference object was destroyed, but the pinger task continued running:

```
Thread 18 (crashed):
  conference::Conference::send_iq()  ← accessing destroyed object
  pinger_main (.resume)
  coop::Runner::run()

Thread 0 (main, blocked):
  std::thread::join()
  ready_to_null()
```

### Wrong Teardown Order

The original teardown in `AppController` was:

```cpp
// BROKEN ORDER:
conference_.reset();                    // 1. Destroy ParticipantManager
gst_element_set_state(pipeline, NULL);  // 2. Stop jitsibin (too late!)
```

This destroyed `ParticipantManager` while the jitsibin runner thread was still emitting signals.

## Changes Implemented

### 1. Plugin Fix: `gstjitsimeet` (Pinger Task Cancellation)

**File:** `gstjitsimeet/src/jitsibin.cpp`

The `pinger_main` coroutine was pushed to the runner as a sibling task with a local `TaskHandle`. When `connection_task` was cancelled, the local handle was destroyed but the pinger task kept running with a dangling reference.

**Fix:** Store `ping_task` in `RealSelf` and cancel it explicitly in `ready_to_null`:

```cpp
struct RealSelf {
    // ... existing fields ...
    coop::TaskHandle   ping_task;  // NEW: persistent handle
    // ...
};

// In connect_to_conference:
self.runner.push_task(pinger_main(*conference), &self.ping_task);

// In ready_to_null:
self.injector.inject_task([](RealSelf& self) -> coop::Async<void> {
    // Cancel pinger first - it holds a reference to conference which
    // will be destroyed when connection_task is cancelled
    self.ping_task.cancel();
    self.ws_task.cancel();
    self.connection_task.cancel();
    self.injector.blocker.stop();
    co_return;
}(self));
```

### 2. Application Fix: `qnujitsi` (Signal Disconnect + Shutdown Flag)

**Files:**
- `participant_manager.h`
- `participant_manager.cpp`
- `conference.h`
- `conference.cpp`
- `app_controller.cpp`

#### 2.1 Atomic Shutdown Flag

Added an atomic flag to `ParticipantManager` that signal handlers check before accessing any state:

```cpp
// participant_manager.h
#include <atomic>

class ParticipantManager {
    // ...
    std::atomic<bool> shuttingDown_{false};
};
```

#### 2.2 Signal Disconnection Method

Added method to disconnect all jitsibin signals and set the shutdown flag:

```cpp
// participant_manager.cpp
void ParticipantManager::disconnectSignals() {
    // Set shutdown flag first - signal handlers will check this and return early
    shuttingDown_.store(true, std::memory_order_release);

    // Disconnect all signals from jitsibin to prevent further callbacks
    if (jitsibin_) {
        g_signal_handlers_disconnect_by_data(jitsibin_, this);
    }
}
```

#### 2.3 Protected Signal Handlers

All signal handlers now check the shutdown flag before proceeding:

```cpp
void ParticipantManager::onMuteStateChanged(GstElement*, const gchar* participant_id,
                                             gboolean is_audio, gboolean new_muted,
                                             gpointer user_data) {
    auto* self = static_cast<ParticipantManager*>(user_data);
    if (!self || self->shuttingDown_.load(std::memory_order_acquire)) return;
    self->handleMuteStateChanged(participant_id, is_audio, new_muted);
}
```

This pattern was applied to all 5 signal handlers:
- `onPadAdded`
- `onParticipantJoined`
- `onParticipantLeft`
- `onMuteStateChanged`
- `onFinished`

#### 2.4 Conference Wrapper

Added `disconnectSignals()` to `Conference` class to forward to `ParticipantManager`:

```cpp
// conference.cpp
void Conference::disconnectSignals() {
    if (participants_) {
        participants_->disconnectSignals();
    }
}
```

#### 2.5 Correct Teardown Order

**File:** `app_controller.cpp`

Fixed the teardown sequence to ensure proper ordering:

```cpp
void AppController::teardown() {
    if (!conference_) return;
    GstElement* pipeline = conference_->pipeline();

    // CRITICAL ORDERING for safe teardown:
    // 1. Disconnect signals first - prevents new callbacks from jitsibin
    conference_->disconnectSignals();

    // 2. Set pipeline to NULL - this stops jitsibin's internal tasks and
    //    waits for the runner thread to finish. After this returns,
    //    no more signals will be emitted.
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
    }

    // 3. Now safe to destroy Conference (ParticipantManager, SendPipeline)
    //    since no callbacks are possible
    conference_.reset();

    // 4. Finally unref the pipeline after Conference is gone
    if (pipeline) {
        gst_object_unref(pipeline);
    }

    // Reset UI state...
}
```

## Why This Works

1. **Atomic shutdown flag** - Even if a signal is mid-flight when we call `disconnectSignals()`, the handler checks the flag and returns early without accessing any member variables.

2. **Signal disconnection** - `g_signal_handlers_disconnect_by_data()` removes all callbacks for this instance, preventing future emissions from reaching our handlers.

3. **Proper ordering** - The pipeline goes to NULL state (which internally waits for jitsibin's runner thread to finish via `runner_thread.join()`) **before** we destroy any objects that the signals reference.

4. **Pinger cancellation** - In jitsibin, the pinger task is explicitly cancelled **before** the connection task, ensuring it stops before the conference object it references is destroyed.

## Sequence Diagram

```
Main Thread                          Runner Thread
    |                                     |
    |  disconnectSignals()                |
    |  └─ shuttingDown_ = true            |
    |  └─ g_signal_disconnect()           |
    |                                     |
    |  gst_element_set_state(NULL)        |
    |  └─ ready_to_null()                 |
    |     └─ ws_context.shutdown()        |
    |     └─ inject cancel task ─────────>|
    |                                     | ping_task.cancel()
    |                                     | ws_task.cancel()
    |                                     | connection_task.cancel()
    |                                     | blocker.stop()
    |     └─ runner_thread.join() <───────| (thread exits)
    |                                     X
    |  conference_.reset()                
    |  └─ ~ParticipantManager()           
    |  └─ ~SendPipeline()                 
    |                                     
    |  gst_object_unref(pipeline)         
    |                                     
    ✓ Safe teardown complete
```

## Result

- Disconnect now completes reliably without crashes or hangs
- Tested 10+ consecutive disconnect/reconnect cycles without issues
- No GStreamer assertions or use-after-free errors

