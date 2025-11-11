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
#include <thread>

#include <gst/gst.h>
#include <gst/gstdebugutils.h>
#include <gst/video/video.h>
#include <gst/video/video-event.h>

#include <vector>

#include "participant_manager.h"
#include "send_pipeline.h"
#include "conference.h"

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);

// Declare the GStreamer QML6 plugin resource initialization function
// This is built into libgstqml6.a and contains the shader resources
extern int qInitResources_resources();

// Receive handling moved into ParticipantManager

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

  // Build conference (pipeline + jitsibin + send + participant signal wiring)
  Conference conf;
  if (!conf.build(host, room, video_width, video_height, 4, 720)) {
    g_printerr("Failed to build conference\n");
    return 1;
  }

  // Load QML and bind the sink to the QML video item
  QQmlApplicationEngine engine;
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) return 1;

  QQuickWindow* root = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  if (!root) return 1;
  // Initialize per-slot receive chains
  if (!conf.initQmlSlots(root)) {
    g_printerr("No QML video slots found (expected videoItem0..N)\n");
    return 1;
  }

  conf.dumpDot("qnujitsi");

  // Start the pipeline in the render thread
  conf.scheduleStart(root);

  int ret = app.exec();

  gst_element_set_state(conf.pipeline(), GST_STATE_NULL);
  gst_object_unref(conf.pipeline());
  // Stop GLib loop and join thread
  if (glibLoop) {
    g_main_loop_quit(glibLoop);
    if (glibThread.joinable()) glibThread.join();
    g_main_loop_unref(glibLoop);
  }
  gst_deinit();
  return ret;
}
