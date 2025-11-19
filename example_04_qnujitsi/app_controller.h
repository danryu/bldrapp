#pragma once

#include <QObject>
#include <memory>

#include "camera_manager.h"

class QQuickWindow;
class Conference;

class AppController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
  Q_PROPERTY(CameraManager* cameraManager READ cameraManager CONSTANT)
public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  Q_INVOKABLE bool connectToConference(QQuickWindow* rootWindow,
                                       const QString& host,
                                       const QString& room,
                                       int videoWidth = 1280,
                                       int videoHeight = 720,
                                       int receiveLimit = 4,
                                       int receiveMaxHeight = 720);

  Q_INVOKABLE void disconnectConference();

  bool isConnected() const;
  CameraManager* cameraManager() const { return cameraManager_.get(); }

signals:
  void connectedChanged();
  void error(const QString& message);

private:
  void teardown();

  std::unique_ptr<Conference> conference_;
  std::unique_ptr<CameraManager> cameraManager_;
};


