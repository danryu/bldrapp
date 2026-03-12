#include "send_pipeline.h"
#include "platform_elements.h"

#include <gst/gst.h>

SendPipeline::SendPipeline(GstElement* pipeline, GstElement* jitsibin)
  : pipeline_(pipeline), jitsibin_(jitsibin) {}

SendPipeline::~SendPipeline() {
  // Set elements to NULL state and remove from pipeline
  auto setNullAndRemove = [this](GstElement* elem) {
    if (elem && pipeline_) {
      gst_element_set_state(elem, GST_STATE_NULL);
      gst_bin_remove(GST_BIN(pipeline_), elem);
    }
  };

  // Remove all video elements
  setNullAndRemove(videosrc_);
  setNullAndRemove(videoconvert_);
  setNullAndRemove(videorate_);
  setNullAndRemove(videoscale_);
  setNullAndRemove(tee_);
  setNullAndRemove(local_queue_);
  setNullAndRemove(vtenc_);
  setNullAndRemove(h264parse_);
  setNullAndRemove(video_queue_);

  // Remove audio elements
  setNullAndRemove(audiosrc_);
  setNullAndRemove(audioconvert_);
  setNullAndRemove(volume_);
  setNullAndRemove(opusenc_);

  g_print("SendPipeline destroyed\n");
}

bool SendPipeline::buildAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert,
                                 GstDevice* cameraDevice, const char* audioDeviceIndex) {
  useCamera_ = (cameraDevice != nullptr);
  if (!createElements(localSlotVideoconvert != nullptr, cameraDevice)) return false;
  configureElements(videoWidth, videoHeight, audioDeviceIndex);
  return addToBinAndLink(videoWidth, videoHeight, localSlotVideoconvert);
}

bool SendPipeline::createElements(bool withLocalPreview, GstDevice* cameraDevice) {
  // Create video source - either from GstDevice or test source
  if (cameraDevice) {
    // Create avfvideosrc from GstDevice (bypasses unstable device-index)
    videosrc_ = gst_device_create_element(cameraDevice, nullptr);
    if (!videosrc_) {
      g_printerr("Failed to create element from camera device\n");
      return false;
    }
    videoconvert_ = gst_element_factory_make("videoconvert", nullptr);
    videorate_ = gst_element_factory_make("videorate", nullptr);
  } else {
    videosrc_ = gst_element_factory_make("videotestsrc", nullptr);
  }

  videoscale_ = gst_element_factory_make("videoscale", nullptr);

  // Create tee and local queue only if we're doing local preview
  if (withLocalPreview) {
    tee_ = gst_element_factory_make("tee", nullptr);
    local_queue_ = gst_element_factory_make("queue", nullptr);
  }

  // Create platform-appropriate video encoder
  VideoEncoderConfig encoderConfig = createPlatformVideoEncoder();
  vtenc_ = encoderConfig.element;
  encoderName_ = encoderConfig.encoder_name;
  isHardwareEncoder_ = encoderConfig.is_hardware;

  h264parse_    = gst_element_factory_make("h264parse", nullptr);
  video_queue_  = gst_element_factory_make("queue", nullptr);

  // Create platform-appropriate audio source
  AudioSourceConfig audioConfig = createPlatformAudioSource();
  audiosrc_ = audioConfig.element;
  audioSourceName_ = audioConfig.source_name;

  audioconvert_ = gst_element_factory_make("audioconvert", nullptr);
  volume_       = gst_element_factory_make("volume", nullptr);
  opusenc_      = gst_element_factory_make("opusenc", nullptr);

  if (!pipeline_ || !jitsibin_ ||
      !videosrc_ || !videoscale_ || !vtenc_ || !h264parse_ || !video_queue_ ||
      !audiosrc_ || !audioconvert_ || !volume_ || !opusenc_) {
    g_printerr("Failed to create one or more send elements.\n");
    return false;
  }

  // Check camera-specific elements
  if (cameraDevice && (!videoconvert_ || !videorate_)) {
    g_printerr("Failed to create videoconvert or videorate for camera source.\n");
    return false;
  }

  // Check tee and local_queue if we're doing local preview
  if (withLocalPreview && (!tee_ || !local_queue_)) {
    g_printerr("Failed to create tee or local_queue for local preview.\n");
    return false;
  }

  return true;
}

void SendPipeline::configureElements(int /*videoWidth*/, int /*videoHeight*/,
                                      const char* audioDeviceIndex) {
  // Configure encoder queue for low-latency (leaky downstream to prevent buffering)
  g_object_set(G_OBJECT(video_queue_),
               "max-size-buffers", 0,
               "max-size-time", 0,
               "max-size-bytes", 0,
               "leaky", 2, // GST_QUEUE_LEAK_DOWNSTREAM
               NULL);

  // Configure tee for async branching if present
  if (tee_) {
    g_object_set(G_OBJECT(tee_),
                 "allow-not-linked", TRUE,
                 NULL);
  }

  // Configure local preview queue if present
  if (local_queue_) {
    g_object_set(G_OBJECT(local_queue_),
                 "max-size-buffers", 5,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 "leaky", 2, // GST_QUEUE_LEAK_DOWNSTREAM - drop old frames
                 "flush-on-eos", TRUE,
                 NULL);
  }

  // Configure platform-specific video encoder for low latency
  VideoEncoderConfig encoderConfig = {vtenc_, encoderName_, isHardwareEncoder_};
  configurePlatformVideoEncoder(encoderConfig, 2500);  // 2.5 Mbps default bitrate

  // h264parse: send SPS/PPS with every IDR
  g_object_set(G_OBJECT(h264parse_),
               "config-interval", 1,
               NULL);

  // Configure video source
  if (useCamera_) {
    // Camera created from GstDevice is already configured with correct device
    // (no configuration needed - GstDevice handles device selection)
  } else {
    // Test source configuration
    g_object_set(G_OBJECT(videosrc_), "is-live", TRUE, NULL);
  }

  // Configure platform-specific audio source device
  AudioSourceConfig audioConfig = {audiosrc_, audioSourceName_};
  configurePlatformAudioSource(audioConfig, audioDeviceIndex);
}

bool SendPipeline::addToBinAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert) {
  // Add to bin
  if (localSlotVideoconvert) {
    // With local preview: add tee and local_queue
    if (useCamera_) {
      gst_bin_add_many(GST_BIN(pipeline_),
                       // video path
                       videosrc_, videoconvert_, videorate_, videoscale_, tee_, local_queue_,
                       vtenc_, h264parse_, video_queue_,
                       // audio path
                       audiosrc_, audioconvert_, volume_, opusenc_,
                       NULL);
    } else {
      gst_bin_add_many(GST_BIN(pipeline_),
                       // video path
                       videosrc_, videoscale_, tee_, local_queue_, vtenc_, h264parse_, video_queue_,
                       // audio path
                       audiosrc_, audioconvert_, volume_, opusenc_,
                       NULL);
    }
  } else {
    // Without local preview
    if (useCamera_) {
      gst_bin_add_many(GST_BIN(pipeline_),
                       // video path
                       videosrc_, videoconvert_, videorate_, videoscale_, vtenc_, h264parse_, video_queue_,
                       // audio path
                       audiosrc_, audioconvert_, volume_, opusenc_,
                       NULL);
    } else {
      gst_bin_add_many(GST_BIN(pipeline_),
                       // video path
                       videosrc_, videoscale_, vtenc_, h264parse_, video_queue_,
                       // audio path
                       audiosrc_, audioconvert_, volume_, opusenc_,
                       NULL);
    }
  }

  g_print("=== Video send pipeline ===\n");
  const char* srcType = useCamera_ ? "camera" : "videotestsrc";
  if (localSlotVideoconvert) {
    if (useCamera_) {
      g_print("%s -> videoconvert -> videorate -> videoscale -> tee\n", srcType);
    } else {
      g_print("%s -> videoscale -> tee\n", srcType);
    }
    g_print("  tee branch 1: queue(leaky) -> videoconvert (slot 0 for local preview)\n");
    g_print("  tee branch 2: %s -> h264parse -> queue(leaky) -> jitsibin:video_sink\n", encoderName_);
  } else {
    if (useCamera_) {
      g_print("%s -> videoconvert -> videorate -> videoscale -> %s -> h264parse -> queue(leaky) -> jitsibin:video_sink\n", srcType, encoderName_);
    } else {
      g_print("%s -> videoscale -> %s -> h264parse -> queue(leaky) -> jitsibin:video_sink\n", srcType, encoderName_);
    }
  }

  // Link video source chain
  GstElement* scaleInput = nullptr;
  GstCaps* raw_caps = nullptr;

  if (useCamera_) {
    // Camera path: videosrc -> videoconvert -> videorate -> videoscale
    // Don't link directly - use caps filters between elements for better negotiation

    // videosrc -> videoconvert (no caps, let camera negotiate)
    if (!gst_element_link(videosrc_, videoconvert_)) {
      g_printerr("Failed to link avfvideosrc -> videoconvert\n");
      return false;
    }

    // videoconvert -> videorate with framerate cap
    GstCaps* rate_caps = gst_caps_new_simple("video/x-raw",
                                             "framerate", GST_TYPE_FRACTION, 30, 1,
                                             NULL);
    if (!gst_element_link_filtered(videoconvert_, videorate_, rate_caps)) {
      g_printerr("Failed to link videoconvert -> videorate with framerate caps\n");
      gst_caps_unref(rate_caps);
      return false;
    }
    gst_caps_unref(rate_caps);

    // videorate -> videoscale with resolution caps
    GstCaps* scale_caps = gst_caps_new_simple("video/x-raw",
                                              "width", G_TYPE_INT, videoWidth,
                                              "height", G_TYPE_INT, videoHeight,
                                              "framerate", GST_TYPE_FRACTION, 30, 1,
                                              NULL);
    if (!gst_element_link_filtered(videorate_, videoscale_, scale_caps)) {
      g_printerr("Failed to link videorate -> videoscale with resolution caps\n");
      gst_caps_unref(scale_caps);
      return false;
    }
    gst_caps_unref(scale_caps);

    scaleInput = videorate_;

    // Create final caps for I420 format after videoscale
    raw_caps = gst_caps_new_simple("video/x-raw",
                                   "format", G_TYPE_STRING, "I420",
                                   "width", G_TYPE_INT, videoWidth,
                                   "height", G_TYPE_INT, videoHeight,
                                   "framerate", GST_TYPE_FRACTION, 30, 1,
                                   NULL);
  } else {
    // Test source path: videosrc -> videoscale
    if (!gst_element_link(videosrc_, videoscale_)) {
      g_printerr("Failed to link videotestsrc -> videoscale\n");
      return false;
    }
    scaleInput = videosrc_;

    // Create caps for raw video (I420 format)
    raw_caps = gst_caps_new_simple("video/x-raw",
                                   "format", G_TYPE_STRING, "I420",
                                   "width", G_TYPE_INT, videoWidth,
                                   "height", G_TYPE_INT, videoHeight,
                                   "framerate", GST_TYPE_FRACTION, 30, 1,
                                   NULL);
  }

  if (localSlotVideoconvert) {
    // Link videoscale -> tee with explicit caps
    if (!gst_element_link_filtered(videoscale_, tee_, raw_caps)) {
      g_printerr("Failed to link videoscale -> tee with filtered caps (I420/%dx%d@30)\n",
                 videoWidth, videoHeight);
      gst_caps_unref(raw_caps);
      return false;
    }
    
    // Branch 1: tee -> local_queue -> localSlotVideoconvert (for local preview)
    GstPad* tee_src_pad_local = gst_element_request_pad_simple(tee_, "src_%u");
    GstPad* queue_sink_pad = gst_element_get_static_pad(local_queue_, "sink");
    if (!tee_src_pad_local || !queue_sink_pad) {
      g_printerr("Failed to get pads for tee -> local_queue link\n");
      gst_caps_unref(raw_caps);
      if (tee_src_pad_local) gst_object_unref(tee_src_pad_local);
      if (queue_sink_pad) gst_object_unref(queue_sink_pad);
      return false;
    }
    if (gst_pad_link(tee_src_pad_local, queue_sink_pad) != GST_PAD_LINK_OK) {
      g_printerr("Failed to link tee -> local_queue\n");
      gst_caps_unref(raw_caps);
      gst_object_unref(tee_src_pad_local);
      gst_object_unref(queue_sink_pad);
      return false;
    }
    gst_object_unref(tee_src_pad_local);
    gst_object_unref(queue_sink_pad);
    
    // Link local_queue -> localSlotVideoconvert
    if (!gst_element_link(local_queue_, localSlotVideoconvert)) {
      g_printerr("Failed to link local_queue -> videoconvert (slot 0)\n");
      gst_caps_unref(raw_caps);
      return false;
    }
    
    // Branch 2: tee -> vtenc (encoder branch)
    GstPad* tee_src_pad_enc = gst_element_request_pad_simple(tee_, "src_%u");
    GstPad* enc_sink_pad = gst_element_get_static_pad(vtenc_, "sink");
    if (!tee_src_pad_enc || !enc_sink_pad) {
      g_printerr("Failed to get pads for tee -> vtenc link\n");
      gst_caps_unref(raw_caps);
      if (tee_src_pad_enc) gst_object_unref(tee_src_pad_enc);
      if (enc_sink_pad) gst_object_unref(enc_sink_pad);
      return false;
    }
    if (gst_pad_link(tee_src_pad_enc, enc_sink_pad) != GST_PAD_LINK_OK) {
      g_printerr("Failed to link tee -> vtenc\n");
      gst_caps_unref(raw_caps);
      gst_object_unref(tee_src_pad_enc);
      gst_object_unref(enc_sink_pad);
      return false;
    }
    gst_object_unref(tee_src_pad_enc);
    gst_object_unref(enc_sink_pad);
  } else {
    // No local preview: Link videoscale -> vtenc with explicit caps
    if (!gst_element_link_filtered(videoscale_, vtenc_, raw_caps)) {
      g_printerr("Failed to link videoscale -> vtenc_h264 with filtered caps (I420/%dx%d@30)\n",
                 videoWidth, videoHeight);
      gst_caps_unref(raw_caps);
      return false;
    }
  }
  
  gst_caps_unref(raw_caps);

  // Link vtenc -> h264parse -> queue (common path for both cases)
  if (!gst_element_link(vtenc_, h264parse_)) {
    g_printerr("Failed to link vtenc_h264 -> h264parse\n");
    return false;
  }
  if (!gst_element_link(h264parse_, video_queue_)) {
    g_printerr("Failed to link h264parse -> queue\n");
    return false;
  }
  // Link queue to jitsibin pad
  if (!gst_element_link_pads(video_queue_, NULL, jitsibin_, "video_sink")) {
    g_printerr("Failed to link queue -> jitsibin video_sink\n");
    return false;
  }

  // Audio path: osxaudiosrc -> audioconvert -> volume -> opusenc -> jitsibin
  if (!gst_element_link_many(audiosrc_, audioconvert_, volume_, opusenc_, NULL)) {
    g_printerr("Failed to link osxaudiosrc -> audioconvert -> volume -> opusenc\n");
    return false;
  }
  if (!gst_element_link_pads(opusenc_, NULL, jitsibin_, "audio_sink")) {
    g_printerr("Failed to link opusenc -> jitsibin audio_sink\n");
    return false;
  }
  g_print("=== Audio send pipeline ===\n");
  g_print("%s (microphone) -> audioconvert -> volume -> opusenc -> jitsibin:audio_sink\n", audioSourceName_);

  return true;
}

bool SendPipeline::syncStateWithParent() {
  // Sync all elements with the parent pipeline state
  // This is necessary when hot-swapping elements in a running pipeline
  auto syncElement = [](GstElement* elem) {
    if (elem) {
      if (!gst_element_sync_state_with_parent(elem)) {
        gchar* name = gst_element_get_name(elem);
        g_printerr("Failed to sync element %s with parent\n", name);
        g_free(name);
        return false;
      }
    }
    return true;
  };

  // Sync all video elements
  if (!syncElement(videosrc_)) return false;
  if (!syncElement(videoconvert_)) return false;
  if (!syncElement(videorate_)) return false;
  if (!syncElement(videoscale_)) return false;
  if (!syncElement(tee_)) return false;
  if (!syncElement(local_queue_)) return false;
  if (!syncElement(vtenc_)) return false;
  if (!syncElement(h264parse_)) return false;
  if (!syncElement(video_queue_)) return false;

  // Sync audio elements
  if (!syncElement(audiosrc_)) return false;
  if (!syncElement(audioconvert_)) return false;
  if (!syncElement(volume_)) return false;
  if (!syncElement(opusenc_)) return false;

  g_print("All send pipeline elements synced with parent state\n");
  return true;
}

bool SendPipeline::setVideoMuted(bool muted, GstDevice* newDevice) {
  if (!videosrc_ || videoMuted_ == muted) return true;

  if (muted) {
    // Lock state so parent changes don't affect it, then stop camera
    gst_element_set_locked_state(videosrc_, TRUE);
    gst_element_set_state(videosrc_, GST_STATE_NULL);
    g_print("Camera muted (stopped)\n");
  } else {
    // Unmuting
    if (useCamera_ && newDevice) {
      // Replace the video source with a fresh one to fix stale handle issues on macOS
      g_print("Replacing camera source with fresh device on unmute...\n");

      // 1. Remove old source (it is already in NULL state from mute)
      gst_element_set_locked_state(videosrc_, FALSE); // Unlock before removing? Actually bin removal handles it
      gst_bin_remove(GST_BIN(pipeline_), videosrc_);

      // 2. Create new source
      videosrc_ = gst_device_create_element(newDevice, nullptr);
      if (!videosrc_) {
        g_printerr("Failed to create new element from camera device during unmute\n");
        return false;
      }

      // 3. Add and link
      gst_bin_add(GST_BIN(pipeline_), videosrc_);
      if (!gst_element_link(videosrc_, videoconvert_)) {
        g_printerr("Failed to link new videosrc -> videoconvert\n");
        return false;
      }

      // 4. Sync state (will bring it to PLAYING)
      if (!gst_element_sync_state_with_parent(videosrc_)) {
        g_printerr("Failed to sync new videosrc state with parent\n");
        return false;
      }

      g_print("Camera unmuted (replaced and restarted)\n");
    } else {
      // Restore existing element
      gst_element_set_locked_state(videosrc_, FALSE);
      gst_element_sync_state_with_parent(videosrc_);
      g_print("Camera unmuted\n");
    }
  }
  videoMuted_ = muted;

  // Signal mute state to jitsibin (which sends presence update to server)
  if (jitsibin_) {
    g_object_set(G_OBJECT(jitsibin_), "video-muted", muted ? TRUE : FALSE, NULL);
  }
  return true;
}

bool SendPipeline::setAudioMuted(bool muted) {
  if (!volume_) return false;
  g_object_set(G_OBJECT(volume_), "mute", muted ? TRUE : FALSE, NULL);
  audioMuted_ = muted;
  g_print("Microphone %s\n", muted ? "muted" : "unmuted");

  // Signal mute state to jitsibin (which sends presence update to server)
  if (jitsibin_) {
    g_object_set(G_OBJECT(jitsibin_), "audio-muted", muted ? TRUE : FALSE, NULL);
  }
  return true;
}
bool SendPipeline::setCamera(GstDevice* newDevice) {
  if (!useCamera_ || !videosrc_ || !newDevice) return false;

  // Don't switch if video is muted (we'll switch on unmute instead)
  if (videoMuted_) {
    // Just update internal state if needed? 
    // Actually, on unmute we rely on CameraManager's current selection, 
    // so we don't need to do anything here if muted.
    g_print("Camera switch requested while muted - will apply on unmute\n");
    return true;
  }

  g_print("Hot-swapping camera source...\n");

  // 1. Lock old source state
  gst_element_set_locked_state(videosrc_, TRUE);
  gst_element_set_state(videosrc_, GST_STATE_NULL);

  // 2. Remove old source from bin
  // Note: gst_bin_remove automatically unrefs the element
  gst_bin_remove(GST_BIN(pipeline_), videosrc_);

  // 3. Create new source from fresh device
  videosrc_ = gst_device_create_element(newDevice, nullptr);
  if (!videosrc_) {
    g_printerr("Failed to create new element from camera device during hot-swap\n");
    return false;
  }

  // 4. Add new source to bin
  gst_bin_add(GST_BIN(pipeline_), videosrc_);

  // 5. Link to videoconvert
  if (!gst_element_link(videosrc_, videoconvert_)) {
    g_printerr("Failed to link new videosrc -> videoconvert during hot-swap\n");
    return false;
  }

  // 6. Sync state with parent (will bring it to PLAYING)
  if (!gst_element_sync_state_with_parent(videosrc_)) {
    g_printerr("Failed to sync new videosrc state with parent during hot-swap\n");
    return false;
  }

  g_print("Camera hot-swap complete\n");
  return true;
}
