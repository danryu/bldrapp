### Jitsi Receive Pipeline Slot-Race Fix

#### Background

`example_04_qnujitsi` uses `ParticipantManager` to manage a fixed set of receive slots, each feeding a `GstGLQt6VideoItem` via:

- `videoconvert -> queue -> glupload -> glcolorconvert -> qml6glsink`

New incoming pads from `jitsibin` are handled by `ParticipantManager::handlePadAdded`, which selects a free slot and wires:

- `jitsibin pad -> h264parse -> decoder -> videoconvert(slot) -> ...`

#### Root Cause

- `pad-added` signals are emitted from GStreamer threads, so multiple `handlePadAdded` calls can run concurrently.
- Slot selection was based on `inUse_` scanned without any locking, and `inUse_[slot]` was only set to `true` **after** all linking succeeded.
- When two pads arrived close together, both callbacks could see `inUse_[0] == false`, both choose slot 0, and:
  - The first links `dec1 -> videoconvert[0]` successfully.
  - The second then attempts `dec2 -> videoconvert[0]`, which is already linked, causing a link failure and the observed intermittent error:
    - “Failed to link decoder / downstream queue -> videoconvert (slot 0)”.

#### Implemented Fix

- **Introduced a mutex** in `ParticipantManager` to protect slot allocation and usage flags:

  - Added `std::mutex slotMutex_;` as a private member.
  - Renamed the helper to `acquireFreeSlotLocked()` to reflect that it must be called under the mutex (or in practice, inlined the logic under the lock in `handlePadAdded`).

- **Reserved slots under lock, before linking:**

  - In `handlePadAdded`, we now:

    1. Acquire `slotMutex_`.
    2. Scan `inUse_` for the first `false`.
    3. Immediately set `inUse_[i] = true` to reserve the slot.
    4. Cache `slot_index` and the slot’s `videoconvert` pointer.
    5. Release the lock and then build/link `h264parse` + decoder into that slot.

- **Released slots on failure:**

  - On any failure after reservation (e.g. failing to link `h264parse -> decoder` or `decoder -> videoconvert`), we:

    - Re-acquire `slotMutex_`.
    - Set `inUse_[slot_index] = false` to make the slot available again.

#### Impact

- **Eliminates the race condition** where multiple pad-added callbacks could attach to the same slot.
- **Removes the intermittent link failures** (“Failed to link decoder/downstream queue -> videoconvert (slot 0)”) seen in ~10% of runs.
- Keeps the overall pipeline structure intact while making slot management **thread-safe and deterministic**.