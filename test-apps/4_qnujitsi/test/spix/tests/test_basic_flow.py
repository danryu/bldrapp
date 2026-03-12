#!/usr/bin/env python3

import os
import sys
import time
import xmlrpc.client


def getenv(name: str, default: str) -> str:
    v = os.environ.get(name)
    return v if v else default


def wait_for_rpc(url: str, timeout_s: float) -> xmlrpc.client.ServerProxy:
    start = time.time()
    last_err = None
    while time.time() - start < timeout_s:
        try:
            s = xmlrpc.client.ServerProxy(url, allow_none=True)
            # A lightweight call that should always exist
            s.system.listMethods()
            return s
        except Exception as e:
            last_err = e
            time.sleep(0.2)
    raise RuntimeError(f"Timed out waiting for Spix RPC at {url} (last error: {last_err})")


def wait_until(pred, timeout_s: float, poll_s: float = 0.1) -> bool:
    start = time.time()
    while time.time() - start < timeout_s:
        if pred():
            return True
        time.sleep(poll_s)
    return pred()


def main() -> int:
    spix_url = getenv("SPIX_URL", "http://localhost:9000")
    root = getenv("SPIX_ROOT", "window")

    host = getenv("JITSI_SERVER", getenv("QNUJITSI_TEST_HOST", "localhost:8000"))
    room = getenv("JITSI_ROOM", getenv("QNUJITSI_TEST_ROOM", "testroom"))

    connect_timeout_s = float(getenv("QNUJITSI_TEST_CONNECT_TIMEOUT_S", "10"))
    participant_timeout_s = float(getenv("QNUJITSI_TEST_PARTICIPANT_TIMEOUT_S", "15"))
    disconnect_timeout_s = float(getenv("QNUJITSI_TEST_DISCONNECT_TIMEOUT_S", "5"))

    print("========================================")
    print("qnujitsi Automated UI Tests (Spix RPC)")
    print("========================================")
    print("")

    s = wait_for_rpc(spix_url, timeout_s=10.0)

    def path(name: str) -> str:
        return f"{root}/{name}"

    # Initial state probe
    assert s.waitForItem(path("connectButton"), 5000), "connectButton not found"
    assert s.waitForItem(path("disconnectButton"), 5000), "disconnectButton not found"

    # --- Test 1: Conference Connection ---
    print("--- Test 1: Conference Connection ---")
    s.setStringProperty(path("hostField"), "text", host)
    s.setStringProperty(path("roomField"), "text", room)
    s.mouseClick(path("connectButton"))

    ok = wait_until(
        lambda: (s.getStringProperty(path("connectButton"), "enabled") == "false")
        and (s.getStringProperty(path("disconnectButton"), "enabled") == "true"),
        timeout_s=connect_timeout_s,
    )
    assert ok, "Timed out waiting for connected UI state"

    ok = wait_until(lambda: s.existsAndVisible(f"{root}/videoItem0"), timeout_s=connect_timeout_s)
    assert ok, "Timed out waiting for local video slot videoItem0 to become visible"
    print("Test 1 PASSED")
    print("")

    # --- Test 2: Audio/Video Both Ways ---
    print("--- Test 2: Audio/Video Both Ways ---")
    remote_slot = {"idx": None}

    def find_remote() -> bool:
        for idx in range(1, 16):
            if s.existsAndVisible(f"{root}/videoItem{idx}"):
                remote_slot["idx"] = idx
                return True
        return False

    ok = wait_until(find_remote, timeout_s=participant_timeout_s)
    assert ok, "Timed out waiting for any remote video slot (videoItem1..15) to become visible"
    print(f"Remote participant appeared in slot videoItem{remote_slot['idx']}")

    # Allow time for visual confirmation of video being received
    print("Waiting 25 seconds for visual confirmation of video...")
    time.sleep(25)

    # Toggle video mute/unmute: verify button text changes
    before = s.getStringProperty(path("muteVideoButton"), "text")
    s.mouseClick(path("muteVideoButton"))
    ok = wait_until(lambda: s.getStringProperty(path("muteVideoButton"), "text") != before, timeout_s=3.0)
    assert ok, "Video mute toggle did not update button text"
    print("Video muted, waiting 3 seconds for visual confirmation...")
    time.sleep(3)

    mid = s.getStringProperty(path("muteVideoButton"), "text")
    s.mouseClick(path("muteVideoButton"))
    ok = wait_until(lambda: s.getStringProperty(path("muteVideoButton"), "text") != mid, timeout_s=3.0)
    assert ok, "Video unmute toggle did not update button text"
    print("Video unmuted, waiting 3 seconds for visual confirmation...")
    time.sleep(3)

    # Toggle audio mute/unmute: verify button text changes
    before = s.getStringProperty(path("muteAudioButton"), "text")
    s.mouseClick(path("muteAudioButton"))
    ok = wait_until(lambda: s.getStringProperty(path("muteAudioButton"), "text") != before, timeout_s=3.0)
    assert ok, "Audio mute toggle did not update button text"
    print("Audio muted, waiting 3 seconds for visual confirmation...")
    time.sleep(3)

    mid = s.getStringProperty(path("muteAudioButton"), "text")
    s.mouseClick(path("muteAudioButton"))
    ok = wait_until(lambda: s.getStringProperty(path("muteAudioButton"), "text") != mid, timeout_s=3.0)
    assert ok, "Audio unmute toggle did not update button text"
    print("Audio unmuted, waiting 3 seconds for visual confirmation...")
    time.sleep(3)

    print("Test 2 PASSED")
    print("")

    # --- Test 3: Disconnection ---
    print("--- Test 3: Disconnection ---")
    s.mouseClick(path("disconnectButton"))
    ok = wait_until(
        lambda: (s.getStringProperty(path("connectButton"), "enabled") == "true")
        and (s.getStringProperty(path("disconnectButton"), "enabled") == "false"),
        timeout_s=disconnect_timeout_s,
    )
    assert ok, "Timed out waiting for disconnected UI state"

    ## videoitem0 stays - it's the local video slot
    # ok = wait_until(lambda: not s.existsAndVisible(f"{root}/videoItem0"), timeout_s=8.0)
    # assert ok, "Timed out waiting for videoItem0 to become hidden after disconnect"
    ok = wait_until(lambda: not s.existsAndVisible(f"{root}/videoItem{remote_slot['idx']}"), timeout_s=8.0)
    assert ok, f"Timed out waiting for videoItem{remote_slot['idx']} to become hidden after disconnect"

    print("Test 3 PASSED")
    print("")

    errors = s.getErrors()
    if errors:
        print("Spix errors:")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("========================================")
    print("All Spix RPC tests PASSED")
    print("========================================")

    # Cleanly ask the app to quit
    s.quit()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as e:
        print(f"[FAIL] {e}", file=sys.stderr)
        raise


