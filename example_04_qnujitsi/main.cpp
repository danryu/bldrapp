//
// qnujitsi main
//
// This app demonstrates sending and receiving media to a Jitsi Meet conference
// using the custom `jitsibin` GStreamer element. Video is rendered via
// `qml6glsink` into a QML item. The receive pipeline uses decodebin which
// automatically selects the appropriate decoder (hardware-accelerated when available)

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QJSEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <thread>

#include <gst/gst.h>
#include <gst/gstdebugutils.h>
#include <gst/video/video.h>
#include <gst/video/video-event.h>

#include <vector>

#include "app_controller.h"

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);

// Declare the GStreamer QML6 plugin resource initialization function
// This is built into libgstqml6.a and contains the shader resources
extern int qInitResources_resources();

// Application entry: minimal setup then loads QML UI
int main(int argc, char* argv[]) {
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

  // Register controller as a QML singleton
  qmlRegisterSingletonType<AppController>("Qnujitsi", 1, 0, "AppController",
    [](QQmlEngine*, QJSEngine*) -> QObject* { return new AppController(); });

  // Load QML UI
  QQmlApplicationEngine engine;
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) return 1;

  int ret = app.exec();

  // Stop GLib loop and join thread
  if (glibLoop) {
    g_main_loop_quit(glibLoop);
    if (glibThread.joinable()) glibThread.join();
    g_main_loop_unref(glibLoop);
  }
  gst_deinit();
  return ret;
}
