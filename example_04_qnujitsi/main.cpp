//
// qnujitsi main
//
// This app demonstrates sending and receiving media to a Jitsi Meet conference
// using the custom `jitsibin` GStreamer element. Video is rendered via
// `qml6glsink` into a QML item. The receive pipeline uses decodebin which
// automatically selects the appropriate decoder (hardware-accelerated when available)

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRunnable>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-event.h>

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);

// Declare the GStreamer QML6 plugin resource initialization function
// This is built into libgstqml6.a and contains the shader resources
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
// dynamic pad handlers
struct Context {
  GstElement* pipeline {nullptr};
  GstElement* videoconvert {nullptr};
};

// decodebin exposes pads depending on the detected codec
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

  // Insert a downstream-leaky queue before videoconvert to absorb bursts and
  // allow decoder reconfigure without stalling the upstream.
  GstElement* q = gst_element_factory_make("queue", nullptr);
  if (!q) return;
  g_object_set(G_OBJECT(q),
               "leaky", 2 /* downstream */, // GStreamer queue leaky enum
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  gst_bin_add(GST_BIN(self->pipeline), q);
  gst_element_sync_state_with_parent(q);

  // Link: pad -> queue -> videoconvert
  GstPad* qsink = gst_element_get_static_pad(q, "sink");
  if (!qsink) return;
  GstPadLinkReturn linkret = gst_pad_link(pad, qsink);
  gst_object_unref(qsink);
  if (linkret != GST_PAD_LINK_OK) {
    g_printerr("Failed to link decodebin pad -> queue\n");
    return;
  }
  if (!gst_element_link(q, self->videoconvert)) {
    g_printerr("Failed to link queue -> videoconvert\n");
    return;
  }
  g_print("decodebin -> queue(leaky) -> videoconvert link OK\n");
}

// When jitsibin exposes a new RTP stream, we create decodebin to
// handle whatever codec arrives - should be H.264
static void jitsibin_pad_added(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->pipeline) return;

  // Create a decodebin for whatever codec arrives
  GstElement* decodebin = gst_element_factory_make("decodebin", nullptr);
  if (!decodebin) { g_printerr("Failed to create decodebin\n"); return; }

  gst_bin_add(GST_BIN(self->pipeline), decodebin);
  // Keep default autoplug selection (prefer vtdec hardware decoder on macOS)
  g_signal_connect(decodebin, "pad-added", GCallback(decodebin_pad_added), self);
  gst_element_sync_state_with_parent(decodebin);

  // Insert an upstream leaky queue before decodebin to absorb bursts from RTP
  GstElement* q_up = gst_element_factory_make("queue", nullptr);
  if (!q_up) { g_printerr("Failed to create upstream queue\n"); return; }
  g_object_set(G_OBJECT(q_up),
               "leaky", 2 /* downstream */,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  gst_bin_add(GST_BIN(self->pipeline), q_up);
  gst_element_sync_state_with_parent(q_up);

  // Link jitsibin pad -> queue
  {
    GstPad* qsink = gst_element_get_static_pad(q_up, "sink");
    if (!qsink) return;
    GstPadLinkReturn linkret = gst_pad_link(pad, qsink);
    gst_object_unref(qsink);
    if (linkret != GST_PAD_LINK_OK) {
      g_printerr("Failed to link jitsibin pad -> upstream queue\n");
      return;
    }
  }
  // Link queue -> decodebin
  if (!gst_element_link(q_up, decodebin)) {
    g_printerr("Failed to link upstream queue -> decodebin\n");
    return;
  }
  g_print("jitsibin -> queue(leaky) -> decodebin link OK\n");
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

// Application entry: builds the pipeline, wires Qt/QML
int main(int argc, char* argv[]) {
  const char* host = nullptr;
  const char* room = nullptr;
  int video_width = 1280;   // Default to 720p
  int video_height = 720;
  
  if (argc >= 3) {
    host = argv[1];
    room = argv[2];
    // Optional resolution parameters
    if (argc >= 5) {
      video_width = atoi(argv[3]);
      video_height = atoi(argv[4]);
    }
  } else {
    g_printerr("Usage: %s <HOST> <ROOM> [WIDTH HEIGHT]\n", argv[0]);
    g_printerr("  Default resolution: 1280x720\n");
    g_printerr("  Example 480p: %s meet.jit.si myroom 854 480\n", argv[0]);
    g_printerr("  Example 720p: %s meet.jit.si myroom 1280 720\n", argv[0]);
    return 1;
  }
  g_print("Configuration: %dx%d video resolution\n", video_width, video_height);

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

  // Send path elements
  GstElement* videotestsrc = gst_element_factory_make("videotestsrc", nullptr);
  GstElement* videoscale = gst_element_factory_make("videoscale", nullptr);
  GstElement* videncdr = gst_element_factory_make("vtenc_h264", nullptr);
  GstElement* h264parse = gst_element_factory_make("h264parse", nullptr);
  GstElement* video_queue = gst_element_factory_make("queue", nullptr);
  GstElement* audiotestsrc = gst_element_factory_make("audiotestsrc", nullptr);
  GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);

  if (!pipeline || !jitsibin || !videoconvert || !glupload || !sink ||
    !videotestsrc || !videoscale ||
    !videncdr || !h264parse || !video_queue ||
    !audiotestsrc || !opusenc) {
    g_printerr("Failed to create one or more GStreamer elements.\n");
    return 1;
  }
  
  // Configure queue for low-latency (leaky downstream to prevent buffering)
  g_object_set(G_OBJECT(video_queue),
              "max-size-buffers", 0, // No buffer limit
              "max-size-time", 0,    // No time limit
              "max-size-bytes", 0,  // No byte limit
              "leaky", 2,           // GST_QUEUE_LEAK_DOWNSTREAM - drop old buffers if downstream is slow
              NULL);
  
  // Configure H.264 encoder for low-latency real-time encoding
  // VideoToolbox encoder should handle 720p30 easily with hardware acceleration
  g_object_set(G_OBJECT(videncdr),
              "realtime", TRUE,
              "allow-frame-reordering", FALSE, // Disable B-frames for lower latency
              NULL);
  
  // Configure h264parse for RTP
  // Note: h264parse will convert avc format to byte-stream if needed by downstream
  g_object_set(G_OBJECT(h264parse),
              "config-interval", 1, // Send SPS/PPS with every IDR
              NULL); 

  // Configure jitsibin (conference details, codec preferences, and behavior)
  g_object_set(G_OBJECT(jitsibin),
               "server", host,
               "room", room,
               "nick", "qnujitsi_user",
               // Ensure sender uses H.264 so jitsibin selects rtph264pay
               "video-codec", 1, /* H264 enum value */
               "receive-limit", 3,
               // Request unlimited receive resolution (-1 -> no constraint, see JVB docs)
               "receive-max-height", 720,
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
                   videotestsrc, videoscale, videncdr, h264parse, video_queue,
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
  
  g_print("=== Video send pipeline ===\n");
  g_print("videotestsrc -> videoscale -> vtenc_h264 -> h264parse -> queue(leaky) -> jitsibin:video_sink\n");
  g_print("Encoder configured: realtime=TRUE, allow-frame-reordering=FALSE\n");

  // Link videotestsrc to videoscale
  if (!gst_element_link(videotestsrc, videoscale)) {
    g_printerr("Failed to link videotestsrc -> videoscale\n");
    return 1;
  }
  
  // Link videoscale to encoder with explicit caps (I420, size, fps)
  GstCaps* send_caps = gst_caps_new_simple("video/x-raw",
                                          "format", G_TYPE_STRING, "I420",
                                          "width", G_TYPE_INT, video_width,
                                          "height", G_TYPE_INT, video_height,
                                          "framerate", GST_TYPE_FRACTION, 30, 1,
                                          NULL);
  if (!gst_element_link_filtered(videoscale, videncdr, send_caps)) {
    g_printerr("Failed to link videoscale -> vtenc_h264 with filtered caps (I420/%dx%d@30)\n", video_width, video_height);
    gst_caps_unref(send_caps);
    return 1;
  }
  gst_caps_unref(send_caps);

  // Link encoder -> h264parse -> queue -> jitsibin
  if (!gst_element_link(videncdr, h264parse)) {
    g_printerr("Failed to link vtenc_h264 -> h264parse\n");
    return 1;
  }
  if (!gst_element_link(h264parse, video_queue)) {
    g_printerr("Failed to link h264parse -> queue\n");
    return 1;
  }
  if (!gst_element_link_pads(video_queue, NULL, jitsibin, "video_sink")) {
    g_printerr("Failed to link queue -> jitsibin video_sink\n");
    return 1;
  }
  
  // Link audio path
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

  int ret = app.exec();

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  gst_deinit();
  return ret;
}
