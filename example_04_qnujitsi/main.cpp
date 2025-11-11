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
#include <thread>

#include <gst/gst.h>
#include <gst/gstdebugutils.h>
#include <gst/video/video.h>
#include <gst/video/video-event.h>

#include <vector>

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

// Context carries pipeline and a pool of per-slot receive elements
struct Context {
  GstElement* pipeline {nullptr};
  std::vector<GstElement*> videoconverts;
  std::vector<GstElement*> gluploads;
  std::vector<GstElement*> glcolorconverts;
  std::vector<GstElement*> sinks;
  std::vector<bool> in_use;
};

// When jitsibin exposes a new RTP stream, we create decodebin to
// handle whatever codec arrives - should be H.264
static void jitsibin_pad_added(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<Context*>(user_data);
  if (!self || !self->pipeline) return;

  // Inspect pad caps to choose the appropriate receive path
  bool is_rtp_h264 = false;
  bool is_raw_h264 = false;
  {
    GstCaps* pcaps = gst_pad_get_current_caps(pad);
    if (!pcaps) pcaps = gst_pad_query_caps(pad, nullptr);
    if (pcaps) {
      char* caps_str = gst_caps_to_string(pcaps);
      g_print("Incoming pad %s caps: %s\n", GST_PAD_NAME(pad), caps_str ? caps_str : "(null)");
      g_free(caps_str);
      if (GstStructure* s = gst_caps_get_structure(pcaps, 0)) {
        const char* name = gst_structure_get_name(s);
        if (name) {
          if (g_strcmp0(name, "application/x-rtp") == 0) {
            const char* enc = gst_structure_get_string(s, "encoding-name");
            if (enc && g_ascii_strcasecmp(enc, "H264") == 0) is_rtp_h264 = true;
          } else if (g_strcmp0(name, "video/x-h264") == 0) {
            is_raw_h264 = true;
          }
        }
      }
      gst_caps_unref(pcaps);
    }
  }
  if (!is_rtp_h264 && !is_raw_h264) {
    // Default to RTP H264 if undetermined; Jitsi typically delivers RTP
    is_rtp_h264 = true;
  }
  g_print("Incoming pad %s: is_rtp_h264=%d is_raw_h264=%d (defaulted if unknown)\n", GST_PAD_NAME(pad), is_rtp_h264, is_raw_h264);

  // Choose a free video slot
  int slot_index = -1;
  for (size_t i = 0; i < self->in_use.size(); ++i) {
    if (!self->in_use[i]) { slot_index = static_cast<int>(i); break; }
  }
  if (slot_index < 0) {
    g_printerr("No available video slots; ignoring new incoming pad %s\n", GST_PAD_NAME(pad));
    return;
  }

  // Insert an upstream leaky queue before depay/parse to absorb bursts
  GstElement* q_up = gst_element_factory_make("queue", nullptr);
  if (!q_up) { g_printerr("Failed to create upstream queue\n"); return; }
  g_object_set(G_OBJECT(q_up),
               "leaky", 2 /* downstream */,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  // Explicit H.264 receive chain: [rtph264depay?] -> h264parse -> vtdec/avdec_h264 -> queue(leaky) -> videoconvert(slot)
  GstElement* depay = is_rtp_h264 ? gst_element_factory_make("rtph264depay", nullptr) : nullptr;
  GstElement* parse = gst_element_factory_make("h264parse", nullptr);
  GstElement* dec = gst_element_factory_make("vtdec", nullptr);
  if (!dec) dec = gst_element_factory_make("avdec_h264", nullptr);
  GstElement* q_down = gst_element_factory_make("queue", nullptr);
  if ((!is_rtp_h264 && !is_raw_h264) || !parse || !dec || !q_down || (is_rtp_h264 && !depay)) {
    g_printerr("Failed to create H264 receive chain elements\n");
    return;
  }
  // Force parser to not passthrough and normalize to avc/au
  g_object_set(G_OBJECT(parse),
               "disable-passthrough", TRUE,
               NULL);
  // Enable realtime on decoders that support it (e.g., vtdec_h264)
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(dec), "realtime")) {
    g_object_set(G_OBJECT(dec), "realtime", TRUE, NULL);
  }
  g_object_set(G_OBJECT(q_down),
               "leaky", 2 /* downstream */,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  if (is_rtp_h264) {
    gst_bin_add_many(GST_BIN(self->pipeline), q_up, depay, parse, dec, q_down, NULL);
  } else {
    gst_bin_add_many(GST_BIN(self->pipeline), q_up, parse, dec, q_down, NULL);
  }
  gst_element_sync_state_with_parent(q_up);
  if (is_rtp_h264) gst_element_sync_state_with_parent(depay);
  gst_element_sync_state_with_parent(parse);
  gst_element_sync_state_with_parent(dec);
  gst_element_sync_state_with_parent(q_down);

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
  // Link: q_up -> [depay] -> parse
  if (is_rtp_h264) {
    if (!gst_element_link(q_up, depay)) {
      g_printerr("Failed to link q_up -> rtph264depay\n");
      return;
    }
    if (!gst_element_link(depay, parse)) {
      g_printerr("Failed to link rtph264depay -> h264parse\n");
      return;
    }
  } else {
    if (!gst_element_link(q_up, parse)) {
      g_printerr("Failed to link q_up -> h264parse (raw h264)\n");
      return;
    }
  }
  // Link parse -> dec with caps enforcing avc/au for vtdec compatibility
  {
    GstCaps* dec_caps = gst_caps_new_simple("video/x-h264",
                                            "stream-format", G_TYPE_STRING, "avc",
                                            "alignment", G_TYPE_STRING, "au",
                                            NULL);
    if (!gst_element_link_filtered(parse, dec, dec_caps)) {
      g_printerr("Failed to link h264parse -> decoder with filtered caps (avc/au)\n");
      gst_caps_unref(dec_caps);
      return;
    }
    gst_caps_unref(dec_caps);
  }
  // Link dec -> q_down
  if (!gst_element_link(dec, q_down)) {
    g_printerr("Failed to link decoder -> downstream queue\n");
    return;
  }
  // Link q_down -> slot videoconvert
  if (!gst_element_link(q_down, self->videoconverts[slot_index])) {
    g_printerr("Failed to link downstream queue -> videoconvert (slot %d)\n", slot_index);
    return;
  }
  g_print("H264 receive chain linked OK (slot %d) via %s\n",
          slot_index, is_rtp_h264 ? "RTP depay" : "raw h264");

  self->in_use[slot_index] = true;

  char dot_name[128];
  g_snprintf(dot_name, sizeof(dot_name), "receive_%s", GST_PAD_NAME(pad));
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(self->pipeline),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            dot_name);
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

  // Run a dedicated GLib main loop to service GLib-based IO and timers
  GMainLoop* glibLoop = g_main_loop_new(nullptr, FALSE);
  std::thread glibThread([glibLoop]{
    g_main_loop_run(glibLoop);
  });

  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

  // Build pipeline elements we know upfront
  GstElement* pipeline = gst_pipeline_new(nullptr);
  GstElement* jitsibin = gst_element_factory_make("jitsibin", nullptr);

  // Send path elements
  GstElement* videotestsrc = gst_element_factory_make("videotestsrc", nullptr);
  GstElement* videoscale = gst_element_factory_make("videoscale", nullptr);
  GstElement* videncdr = gst_element_factory_make("vtenc_h264", nullptr);
  GstElement* h264parse = gst_element_factory_make("h264parse", nullptr);
  GstElement* video_queue = gst_element_factory_make("queue", nullptr);
  GstElement* audiotestsrc = gst_element_factory_make("audiotestsrc", nullptr);
  GstElement* opusenc = gst_element_factory_make("opusenc", nullptr);

  if (!pipeline || !jitsibin ||
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
               "receive-limit", 4,
               // Request unlimited receive resolution (-1 -> no constraint, see JVB docs)
               "receive-max-height", 720,
               "force-play", TRUE,
               "insecure", TRUE,
               NULL);

  // Add elements to the pipeline (dynamic receive path elements are added
  // later when pads appear)
  gst_bin_add_many(GST_BIN(pipeline),
                   jitsibin,
                   // send path
                   videotestsrc, videoscale, videncdr, h264parse, video_queue,
                   audiotestsrc, opusenc,
                   NULL);

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
  Context ctx{};
  ctx.pipeline = pipeline;
  g_signal_connect(jitsibin, "pad-added", GCallback(jitsibin_pad_added), &ctx);
  g_signal_connect(jitsibin, "finished", GCallback(jitsibin_finished), &ctx);

  // Load QML and bind the sink to the QML video item
  QQmlApplicationEngine engine;
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) return 1;

  QQuickWindow* root = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  if (!root) return 1;
  // Create per-slot receive chains for each predeclared QML item: videoItem0, videoItem1, ...
  for (int i = 0; ; ++i) {
    QString objName = QString("videoItem%1").arg(i);
    QQuickItem* item = root->findChild<QQuickItem*>(objName.toUtf8().constData());
    if (!item) break;

    GstElement* vconv = gst_element_factory_make("videoconvert", nullptr);
    GstElement* glup = gst_element_factory_make("glupload", nullptr);
    GstElement* glcc = gst_element_factory_make("glcolorconvert", nullptr);
    GstElement* vsink = gst_element_factory_make("qml6glsink", nullptr);
    if (!vconv || !glup || !glcc || !vsink) {
      g_printerr("Failed to create receive elements for slot %d\n", i);
      return 1;
    }
    gst_bin_add_many(GST_BIN(pipeline), vconv, glup, glcc, vsink, NULL);
    if (!gst_element_link_many(vconv, glup, glcc, vsink, NULL)) {
      g_printerr("Failed to link videoconvert -> glupload -> glcolorconvert -> qml6glsink for slot %d\n", i);
      return 1;
    }
    g_object_set(G_OBJECT(vsink),
                 "widget", item,
                 "sync", FALSE,
                 "async", FALSE,
                 "qos", TRUE,
                 NULL);

    ctx.videoconverts.push_back(vconv);
    ctx.gluploads.push_back(glup);
    ctx.glcolorconverts.push_back(glcc);
    ctx.sinks.push_back(vsink);
    ctx.in_use.push_back(false);
  }
  g_print("Prepared %zu video slots\n", ctx.sinks.size());
  if (ctx.sinks.empty()) {
    g_printerr("No QML video slots found (expected videoItem0..N)\n");
    return 1;
  }

  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            "qnujitsi");

  // Start the pipeline in the render thread
  root->scheduleRenderJob(new SetPlaying(pipeline), QQuickWindow::BeforeSynchronizingStage);

  int ret = app.exec();

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  // Stop GLib loop and join thread
  if (glibLoop) {
    g_main_loop_quit(glibLoop);
    if (glibThread.joinable()) glibThread.join();
    g_main_loop_unref(glibLoop);
  }
  gst_deinit();
  return ret;
}
