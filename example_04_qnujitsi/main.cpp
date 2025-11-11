//
// qnujitsi main
//
// This app demonstrates sending and receiving media to a Jitsi Meet conference
// using the custom `jitsibin` GStreamer element. Video is rendered via
// `qml6glsink` into a QML item. The receive pipeline uses decodebin which
// automatically selects the appropriate decoder (hardware-accelerated when available)

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
#include "app_controller.h"

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);

// Declare the GStreamer QML6 plugin resource initialization function
// This is built into libgstqml6.a and contains the shader resources
extern int qInitResources_resources();

// Receive handling moved into ParticipantManager

// Application entry: builds the pipeline, wires Qt/QML
int main(int argc, char* argv[]) {
  const char* host = nullptr;
  int video_width = 1280;   // Default to 720p
  int video_height = 720;
  
  // Optional CLI: <HOST> [WIDTH HEIGHT]
  if (argc >= 2) {
    host = argv[1];
  }
  if (argc >= 4) {
    video_width = atoi(argv[2]);
    video_height = atoi(argv[3]);
  }
  g_print("Configuration: %dx%d video resolution%s\n",
          video_width, video_height, host ? " (host provided via CLI)" : "");

  gst_init(&argc, &argv);
  // Register static plugins compiled into the libgstreamer-full build
  gst_init_static_plugins();

  QGuiApplication app(argc, argv);
  // Initialize GStreamer QML6 shader resources from libgstqml6.a AFTER QGuiApplication
  // This ensures the Qt resource system has the shaders registered when needed
  qInitResources_resources();

  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

  // Load QML and bind the sink to the QML video item
  QQmlApplicationEngine engine;
  // Expose controller to QML
  AppController controller;
  engine.rootContext()->setContextProperty("controller", &controller);
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) return 1;

  QQuickWindow* root = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  if (!root) return 1;
  controller.setRoot(root);
  // Optional: auto-connect if args passed
  if (host) {
    controller.connectTo(host);
  }

  int ret = app.exec();

  gst_deinit();
  return ret;
}
