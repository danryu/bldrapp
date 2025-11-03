#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRunnable>

#include <gst/gst.h>
#include <gst/video/video.h>

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

struct Context {
  GstElement* pipeline {nullptr};
  GstElement* videoconvert {nullptr};
};

static void decodebin_pad_added(GstElement* /*decodebin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->videoconvert) return;

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

  GstPad* sinkpad = gst_element_get_static_pad(self->videoconvert, "sink");
  if (!sinkpad) return;
  if (gst_pad_is_linked(sinkpad)) { gst_object_unref(sinkpad); return; }

  GstPadLinkReturn linkret = gst_pad_link(pad, sinkpad);
  gst_object_unref(sinkpad);
  g_print("decodebin -> videoconvert link %s\n", linkret == GST_PAD_LINK_OK ? "OK" : "FAILED");
}

static void jitsibin_pad_added(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->pipeline) return;

  // Create a decodebin for whatever codec arrives
  GstElement* decodebin = gst_element_factory_make("decodebin", nullptr);
  if (!decodebin) { g_printerr("Failed to create decodebin\n"); return; }

  gst_bin_add(GST_BIN(self->pipeline), decodebin);
  g_signal_connect(decodebin, "pad-added", GCallback(decodebin_pad_added), self);
  gst_element_sync_state_with_parent(decodebin);

  // Link jitsibin pad -> decodebin sink
  GstPad* dbinsink = gst_element_get_static_pad(decodebin, "sink");
  if (!dbinsink) return;
  GstPadLinkReturn linkret = gst_pad_link(pad, dbinsink);
  gst_object_unref(dbinsink);
  g_print("jitsibin -> decodebin link %s\n", linkret == GST_PAD_LINK_OK ? "OK" : "FAILED");
}

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
  gst_init_static_plugins();  // Register static plugins



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
  GstElement* audiotestsrc = gst_element_factory_make("audiotestsrc", nullptr);
  GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);

  if (!pipeline || !jitsibin || !videoconvert || !glupload || !sink ||
      !videotestsrc || !videoconvert_send || !av1enc || !audiotestsrc || !opusenc) {
    g_printerr("Failed to create one or more GStreamer elements.\n");
    return 1;
  }

  // Set jitsibin properties
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

  gst_bin_add_many(GST_BIN(pipeline),
                   jitsibin,
                   // receive path
                   videoconvert, glupload, sink,
                   // send path
                   videotestsrc, videoconvert_send, av1enc,
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

  if (!gst_element_link_many(videotestsrc, videoconvert_send, av1enc, NULL)) {
    g_printerr("Failed to link videotestsrc -> videoconvert -> av1enc\n");
    return 1;
  }
  if (!gst_element_link_pads(av1enc, NULL, jitsibin, "video_sink")) {
    g_printerr("Failed to link av1enc -> jitsibin video_sink\n");
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

  int ret = app.exec();

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  gst_deinit();
  return ret;
}
