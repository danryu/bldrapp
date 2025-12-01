# Increase Video Slots to 16 (4x4 Grid)

This document describes the changes made to increase the maximum number of video participants from 4 (2x2 grid) to 16 (4x4 grid) in qnujitsi.

## Overview

Previously, qnujitsi supported a maximum of 4 video slots: 1 for local preview and 3 for remote participants, displayed in a 2x2 grid. This change extends support to 16 total slots (1 local + 15 remote) displayed in a 4x4 grid layout.

## Changes

### 1. QML UI Layout

**File:** `main.qml`

- Added `VideoSlot` components for `videoItem4` through `videoItem15` (16 total slots)
- Changed grid layout from dynamic 2-column to fixed 4-column layout
- Updated `activeSlots` array to include all 16 slot info objects
- Updated `connectToConference` call to use `receiveLimit: 15` instead of `4`

### 2. AppController Slot Management

**Files:** `app_controller.h`, `app_controller.cpp`

- Added Q_PROPERTY declarations for slots 4-15
- Added getter methods for all 16 slots
- Added member variables for all 16 `ParticipantInfo` objects
- Updated initialization to create all 16 `ParticipantInfo` instances
- Updated `setParticipantInfoSlots` call to pass all 16 slots
- Updated teardown to reset all 16 slots
- Changed default `receiveLimit` parameter from 4 to 15

### 3. Conference and ParticipantManager

**Files:** `conference.h`, `conference.cpp`, `participant_manager.h`, `participant_manager.cpp`

- Updated `setParticipantInfoSlots` method signatures to accept 16 slots instead of 4
- Updated implementation to store all 16 participant info pointers
- Updated comment from "Slots 1-3" to "Slots 1-15"

### 4. Documentation

**File:** `README.md`

- Updated slot allocation documentation (1-15 remote slots)
- Updated pipeline diagram comments
- Updated configuration documentation (`receive-limit` from 4 to 15)

## How It Works

The `ParticipantManager::initializeSlots()` function dynamically discovers QML video items by searching for `videoItem0`, `videoItem1`, etc. until no more items are found. This means no changes were needed to the slot discovery logic—it automatically works with all 16 slots once the QML items are declared.

Slot allocation remains thread-safe via `slotMutex_`, and remote participants are dynamically assigned to available slots (1-15) when their video streams arrive via jitsibin's `pad-added` signal.

