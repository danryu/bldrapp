// AppController: QML-facing controller to connect/disconnect conference
#pragma once

#include <QObject>
#include <thread>

struct _GMainLoop;
typedef struct _GMainLoop GMainLoop;

class QQuickWindow;
class Conference;

class AppController : public QObject {
  Q_OBJECT
public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  Q_INVOKABLE void setRoot(QObject* windowObject);
  Q_INVOKABLE void connectTo(const QString& host);
  Q_INVOKABLE void disconnect();

private:
  void startGlibLoop();
  void stopGlibLoop();

private:
  QQuickWindow* root_ {nullptr};
  Conference* conf_ {nullptr};
  GMainLoop* glibLoop_ {nullptr};
  std::thread glibThread_;

  // Defaults
  int videoWidth_ {1280};
  int videoHeight_ {720};
  int receiveLimit_ {4};
  int receiveMaxHeight_ {720};
};


