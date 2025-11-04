//
// qjsink main
//
// This app demonstrates sending and receiving media to a Jitsi Meet conference
// using the custom `jitsibin` GStreamer element. Video is rendered via
// `qml6glsink` into a QML item. The receive pipeline is built dynamically to
// accommodate whatever codec the remote sends (AV1, VP9, VP8, H264), and is
// deliberately designed to be resilient to corrupt frames and transient stalls.
//
// Key concepts and decisions:
// - We insert two leaky queues around decode to isolate the network from the
//   decoder and the decoder from the renderer. This prevents stalls from
//   propagating back and freezing the pipeline.
// - We continuously drain the GstBus on a modest cadence (~60 Hz) with a
//   per-tick cap to avoid queue overflow without spinning the CPU.
// - A decoded-frame watchdog requests a new keyframe if no decoded buffers
//   arrive for ~700 ms, which recovers the decoder state after corruption.
// - Upstream keyframe requests are sent via the pre-decode queue (preferred)
//   or directly to jitsibin as a fallback, throttled to ≤2/sec.
//
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRunnable>
#include <QTimer>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-event.h>

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);

// Declare the GStreamer QML6 plugin resource initialization function
// This is built into libgstqml6.a and contains the shader resources
// Note: This is a C++ function (mangled), not extern "C"
extern int qInitResources_resources();

class SetPlaying : public QRunnable {
public:
  explicit SetPlaying(GstElement* pipeline)
      : pipeline_(pipeline ? GST_ELEMENT(gst_object_ref(pipeline)) : nullptr) {}
  ~SetPlaying() override {
    if (pipeline_) gst_object_unref(pipeline_);
  }
  void run() override {
    if (pipeline_) gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  }
private:
  GstElement* pipeline_;
};

// Context carries shared element references and timing state used by
// dynamic pad handlers and timers to implement resilience and monitoring.
struct Context {
  GstElement* pipeline {nullptr};
  GstElement* videoconvert {nullptr};
  // queue inserted between jitsibin and decodebin to isolate decoder; used to send upstream events
  GstElement* predecode_queue {nullptr};
  GstClockTime last_keyunit_request_ts {0};
  // queue between decodebin and videoconvert to monitor flow
  GstElement* postdecode_queue {nullptr};
  GstClockTime last_video_buf_ts {0};
};

// decodebin exposes pads depending on the detected codec. For video streams
// we insert a downstream-leaky queue before videoconvert to prevent render
// stalls from backing up decode, and we probe its src pad to observe decoded
// buffer flow for the watchdog.
static void decodebin_pad_added(GstElement* /*decodebin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->videoconvert || !self->pipeline) return;

  GstCaps* caps = gst_pad_get_current_caps(pad);
  bool is_video = false;
  if (caps) {
    GstStructure* s = gst_caps_get_structure(caps, 0);
    if (s) {
      const char* name = gst_structure_get_name(s);
      if (name && g_str_has_prefix(name, "video/")) is_video = true;
    }
    gst_caps_unref(caps);
  }
  if (!is_video) return;

  GstElement* q = gst_element_factory_make("queue", nullptr);
  if (!q) { g_printerr("Failed to create queue\n"); return; }
  g_object_set(q,
               "leaky", 2,
               "max-size-buffers", 5,
               "max-size-bytes", 0,
               "max-size-time", (guint64)0,
               NULL);

  gst_bin_add(GST_BIN(self->pipeline), q);
  gst_element_sync_state_with_parent(q);

  // remember and probe for buffer flow
  self->postdecode_queue = q;
  GstPad* qsrc = gst_element_get_static_pad(q, "src");
  if (qsrc) {
    gst_pad_add_probe(qsrc, GST_PAD_PROBE_TYPE_BUFFER,
      [](GstPad* /*pad*/, GstPadProbeInfo* /*info*/, gpointer user_data) -> GstPadProbeReturn {
        auto* ctx = static_cast<Context*>(user_data);
        ctx->last_video_buf_ts = gst_util_get_timestamp();
        return GST_PAD_PROBE_OK;
      }, self, NULL);
    gst_object_unref(qsrc);
  }

  GstPad* qsink = gst_element_get_static_pad(q, "sink");
  if (!qsink) return;
  if (gst_pad_is_linked(qsink)) { gst_object_unref(qsink); return; }
  GstPadLinkReturn linkret = gst_pad_link(pad, qsink);
  gst_object_unref(qsink);
  g_print("decodebin -> queue link %s\n", linkret == GST_PAD_LINK_OK ? "OK" : "FAILED");

  if (!gst_element_link(q, self->videoconvert)) {
    g_printerr("Failed to link queue -> videoconvert\n");
  }
}

// When jitsibin exposes a new RTP stream, we create a pre-decode downstream-
// leaky queue to isolate the live network from decode, and a decodebin to
// handle whatever codec arrives. The pre-decode queue is also used as the
// preferred target for upstream force-key-unit events (PLI/FIR).
static void jitsibin_pad_added(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->pipeline) return;

  // Create a leaky queue and a decodebin for whatever codec arrives
  GstElement* q = gst_element_factory_make("queue", nullptr);
  GstElement* decodebin = gst_element_factory_make("decodebin", nullptr);
  if (!q || !decodebin) { g_printerr("Failed to create queue/decodebin\n"); return; }

  g_object_set(q,
               "leaky", 2,
               "max-size-buffers", 50,
               "max-size-bytes", 0,
               "max-size-time", (guint64)0,
               NULL);

  gst_bin_add_many(GST_BIN(self->pipeline), q, decodebin, NULL);
  g_signal_connect(decodebin, "pad-added", GCallback(decodebin_pad_added), self);

  gst_element_sync_state_with_parent(q);
  gst_element_sync_state_with_parent(decodebin);

  // store for upstream keyframe requests
  self->predecode_queue = q;

  // Link jitsibin pad -> queue sink
  GstPad* qsink = gst_element_get_static_pad(q, "sink");
  if (!qsink) return;
  GstPadLinkReturn linkret = gst_pad_link(pad, qsink);
  gst_object_unref(qsink);
  g_print("jitsibin -> queue link %s\n", linkret == GST_PAD_LINK_OK ? "OK" : "FAILED");

  // queue -> decodebin
  if (!gst_element_link(q, decodebin)) {
    g_printerr("Failed to link queue -> decodebin\n");
  }
}

// Surface completion from jitsibin by posting EOS so the app can exit
// gracefully (or react however it chooses).
static void jitsibin_finished(GstElement* /*jitsibin*/, gboolean success, gpointer user_data) {
  g_print("finished success=%d\n", success);
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->pipeline) return;
  GstBus* bus = gst_element_get_bus(self->pipeline);
  if (bus) {
    gst_bus_post(bus, gst_message_new_eos(nullptr));
    gst_object_unref(bus);
  }
}

// Application entry: builds the pipeline, wires Qt/QML and starts timers for
// bus draining and keyframe watchdog.
int main(int argc, char* argv[]) {
  const char* host = nullptr;
  const char* room = nullptr;
  if (argc >= 3) {
    host = argv[1];
    room = argv[2];
  } else {
    g_printerr("Usage: %s <HOST> <ROOM>\n", argv[0]);
    return 1;
  }

  gst_init(&argc, &argv);
  // Register static plugins compiled into the libgstreamer-full build
  gst_init_static_plugins();



  QGuiApplication app(argc, argv);
  // Initialize GStreamer QML6 shader resources from libgstqml6.a AFTER QGuiApplication
  // This ensures the Qt resource system has the shaders registered when needed
  qInitResources_resources();

  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

  // Build pipeline elements we know upfront
  GstElement* pipeline = gst_pipeline_new(nullptr);
  GstElement* jitsibin = gst_element_factory_make("jitsibin", nullptr);

  // Receive path elements
  GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);
  GstElement* glupload = gst_element_factory_make("glupload", nullptr);
  GstElement* sink = gst_element_factory_make("qml6glsink", nullptr);

  // Send path elements (mirror receiver.cpp behavior)
  GstElement* videotestsrc = gst_element_factory_make("videotestsrc", nullptr);
  GstElement* videoconvert_send = gst_element_factory_make("videoconvert", nullptr);
  GstElement* av1enc = gst_element_factory_make("av1enc", nullptr);
  GstElement* av1parse = gst_element_factory_make("av1parse", nullptr);
  GstElement* audiotestsrc = gst_element_factory_make("audiotestsrc", nullptr);
  GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);

  if (!pipeline || !jitsibin || !videoconvert || !glupload || !sink ||
      !videotestsrc || !videoconvert_send || !av1enc || !av1parse || !audiotestsrc || !opusenc) {
    g_printerr("Failed to create one or more GStreamer elements.\n");
    return 1;
  }

  // Configure jitsibin (conference details, codec preferences, and behavior)
  g_object_set(G_OBJECT(jitsibin),
               "server", host,
               "room", room,
               "nick", "qjsink_user",
               // Ensure sender uses AV1 so jitsibin selects rtpav1pay
               "video-codec", 4, /* Av1 enum value */
               "receive-limit", 3,
               "force-play", TRUE,
               "insecure", TRUE,
               NULL);

  // Add elements to the pipeline (dynamic receive path elements are added
  // later when pads appear)
  gst_bin_add_many(GST_BIN(pipeline),
                   jitsibin,
                   // receive path
                   videoconvert, glupload, sink,
                   // send path
                   videotestsrc, videoconvert_send, av1enc, av1parse,
                   audiotestsrc, opusenc,
                   NULL);

  // Link static tail: videoconvert -> glupload -> qml6glsink
  if (!gst_element_link_many(videoconvert, glupload, sink, NULL)) {
    g_printerr("Failed to link videoconvert -> glupload -> qml6glsink\n");
    return 1;
  }

  // Configure and link send path to publish local test AV
  g_object_set(G_OBJECT(videotestsrc), "is-live", TRUE, NULL);
  g_object_set(G_OBJECT(audiotestsrc), "is-live", TRUE, "wave", 8, NULL);
  // Configure AV1 encoder for real-time streaming
  // Use low-latency settings comparable to the previous H.264 setup
  g_object_set(G_OBJECT(av1enc),
               "keyframe-max-dist", 30,
               "cpu-used", 8,
               "lag-in-frames", 0,
               NULL);

  if (!gst_element_link_many(videotestsrc, videoconvert_send, av1enc, av1parse, NULL)) {
    g_printerr("Failed to link videotestsrc -> videoconvert -> av1enc -> av1parse\n");
    return 1;
  }
  if (!gst_element_link_pads(av1parse, NULL, jitsibin, "video_sink")) {
    g_printerr("Failed to link av1parse -> jitsibin video_sink\n");
    return 1;
  }
  if (!gst_element_link_many(audiotestsrc, opusenc, NULL)) {
    g_printerr("Failed to link audiotestsrc -> opusenc\n");
    return 1;
  }
  if (!gst_element_link_pads(opusenc, NULL, jitsibin, "audio_sink")) {
    g_printerr("Failed to link opusenc -> jitsibin audio_sink\n");
    return 1;
  }

  // Connect jitsibin signals to dynamically handle pads
  Context ctx{pipeline, videoconvert};
  g_signal_connect(jitsibin, "pad-added", GCallback(jitsibin_pad_added), &ctx);
  g_signal_connect(jitsibin, "finished", GCallback(jitsibin_finished), &ctx);

  // Load QML and bind the sink to the QML video item
  QQmlApplicationEngine engine;
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) return 1;

  QQuickWindow* root = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  if (!root) return 1;
  QQuickItem* videoItem = root->findChild<QQuickItem*>("videoItem");
  if (!videoItem) return 1;

  g_object_set(G_OBJECT(sink), "widget", videoItem, NULL);

  // Start the pipeline in the render thread
  root->scheduleRenderJob(new SetPlaying(pipeline), QQuickWindow::BeforeSynchronizingStage);

  // Drain the bus periodically to prevent queue overflow. On decode errors,
  // request an upstream keyframe (throttled) to recover decoder state.
  GstBus* bus = gst_element_get_bus(pipeline);
  QTimer* busTimer = new QTimer(&app);
  QObject::connect(busTimer, &QTimer::timeout, [&](){
    int processed = 0;
    constexpr int kMaxPerTick = 200; // cap to keep CPU in check
    for (;;) {
      GstMessage* msg = gst_bus_pop(bus);
      if (!msg) break;

      switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
          GError* err = nullptr; gchar* dbg = nullptr;
          gst_message_parse_error(msg, &err, &dbg);
          g_printerr("ERROR from %s: %s\n", GST_OBJECT_NAME(msg->src), err ? err->message : "unknown");

          if (err && err->domain == GST_STREAM_ERROR && err->code == GST_STREAM_ERROR_DECODE) {
            const GstClockTime now = gst_util_get_timestamp();
            // throttle to avoid spamming RTCP: at most twice per second
            if (now - ctx.last_keyunit_request_ts >= 500 * GST_MSECOND) {
              gboolean sent = FALSE;
              if (ctx.predecode_queue) {
                auto* ev = gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0);
                sent = gst_element_send_event(ctx.predecode_queue, ev);
                g_print("Requested keyframe via predecode queue: %s\n", sent ? "OK" : "FAILED");
              }
              if (!sent) {
                auto* ev2 = gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0);
                sent = gst_element_send_event(jitsibin, ev2);
                g_print("Requested keyframe via jitsibin: %s\n", sent ? "OK" : "FAILED");
              }
              if (sent) ctx.last_keyunit_request_ts = now;
            }
          }

          g_clear_error(&err); g_free(dbg);
          break;
        }
        case GST_MESSAGE_EOS:
          app.quit();
          break;
        default:
          break;
      }
      gst_message_unref(msg);
      if (++processed >= kMaxPerTick) break;
    }
  });
  busTimer->start(16); // ~60Hz is enough to keep the bus drained

  // Watchdog: if no decoded frames observed for a while, request a keyframe upstream
  ctx.last_video_buf_ts = gst_util_get_timestamp();
  QTimer* watchdogTimer = new QTimer(&app);
  QObject::connect(watchdogTimer, &QTimer::timeout, [&](){
    const GstClockTime now = gst_util_get_timestamp();
    // if we haven't seen a decoded buffer for 700ms, ask for a keyframe
    if (now - ctx.last_video_buf_ts > 700 * GST_MSECOND) {
      if (now - ctx.last_keyunit_request_ts >= 500 * GST_MSECOND) {
        gboolean sent = FALSE;
        if (ctx.predecode_queue) {
          auto* ev = gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0);
          sent = gst_element_send_event(ctx.predecode_queue, ev);
          g_print("Watchdog: requested keyframe via predecode queue: %s\n", sent ? "OK" : "FAILED");
        }
        if (!sent) {
          auto* ev2 = gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0);
          sent = gst_element_send_event(jitsibin, ev2);
          g_print("Watchdog: requested keyframe via jitsibin: %s\n", sent ? "OK" : "FAILED");
        }
        if (sent) ctx.last_keyunit_request_ts = now;
      }
    }
  });
  watchdogTimer->start(200);

  int ret = app.exec();

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  if (bus) gst_object_unref(bus);
  gst_deinit();
  return ret;
}
