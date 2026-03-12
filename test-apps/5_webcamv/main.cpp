#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRunnable>
#include <QQmlContext>
#include <gst/gst.h>
#include "camera_manager.h"

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);
extern int qInitResources_resources();

class SetPlaying : public QRunnable
{
public:
  SetPlaying(GstElement * pipeline) {
    pipeline_ = pipeline ? static_cast<GstElement *>(gst_object_ref(pipeline)) : NULL;
  }
  
  ~SetPlaying() {
    if (pipeline_) gst_object_unref(pipeline_);
  }

  void run() override {
    if (pipeline_) gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  }

private:
  GstElement * pipeline_;
};

class WebcamApp : public QObject {
    Q_OBJECT
public:
    WebcamApp(QObject *parent = nullptr) : QObject(parent) {
        connect(&m_cameraManager, &CameraManager::cameraDeviceSelectionChanged, 
                this, &WebcamApp::restartPipeline);
    }

    ~WebcamApp() {
        stopPipeline();
    }

    void initialize(QQmlApplicationEngine* engine) {
        engine->rootContext()->setContextProperty("cameraManager", &m_cameraManager);
        m_cameraManager.enumerateCameras();
    }

    void setVideoItem(QQuickItem* item, QQuickWindow* window) {
        m_videoItem = item;
        m_window = window;
        startPipeline();
    }

public slots:
    void restartPipeline() {
        stopPipeline();
        startPipeline();
    }

private:
    CameraManager m_cameraManager;
    GstElement* m_pipeline = nullptr;
    QQuickItem* m_videoItem = nullptr;
    QQuickWindow* m_window = nullptr;

    void startPipeline() {
        if (!m_videoItem || !m_window) return;

        // Refresh camera list to get fresh GstDevice handles (critical for macOS)
        m_cameraManager.enumerateCameras();
        
        // If no camera selected (or available), don't start pipeline
        GstElement *src = m_cameraManager.createCurrentCameraElement();
        if (!src) {
            qWarning() << "No camera selected or available";
            return;
        }

        m_pipeline = gst_pipeline_new(NULL);
        
        GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
        GstElement *glupload = gst_element_factory_make("glupload", NULL);
        GstElement *queue = gst_element_factory_make("queue", NULL);
        GstElement *sink = gst_element_factory_make("qml6glsink", NULL);

        if (!m_pipeline || !capsfilter || !glupload || !queue || !sink) {
            qWarning() << "Failed to create pipeline elements";
            if (m_pipeline) gst_object_unref(m_pipeline);
            if (src) gst_object_unref(src); 
            // other elements are floating and leaked if not added to bin, 
            // but this is fatal error path anyway
            m_pipeline = nullptr;
            return;
        }
        
        // Configure caps
        GstCaps *caps = gst_caps_from_string("video/x-raw");
        g_object_set(capsfilter, "caps", caps, NULL);
        gst_clear_caps(&caps);

        // Build pipeline
        gst_bin_add_many(GST_BIN(m_pipeline), src, capsfilter, glupload, queue, sink, NULL);
        
        if (!gst_element_link_many(src, capsfilter, glupload, queue, sink, NULL)) {
            qWarning() << "Failed to link pipeline elements";
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            return;
        }

        // Connect to UI
        g_object_set(sink, "widget", m_videoItem, NULL);

        // Start playing
        m_window->scheduleRenderJob(new SetPlaying(m_pipeline),
            QQuickWindow::BeforeSynchronizingStage);
    }

    void stopPipeline() {
        if (m_pipeline) {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
        }
    }
};

#include "main.moc"

int main(int argc, char *argv[])
{
  gst_init(&argc, &argv);
  gst_init_static_plugins();

  int ret;
  {
    QGuiApplication app(argc, argv);
    qInitResources_resources();
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    WebcamApp webcamApp;
    QQmlApplicationEngine engine;
    
    webcamApp.initialize(&engine);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    
    if (engine.rootObjects().isEmpty())
        return -1;

    QQuickWindow *rootObject = static_cast<QQuickWindow *>(engine.rootObjects().first());
    QQuickItem *videoItem = rootObject->findChild<QQuickItem *>("videoItem");
    
    if (videoItem) {
        webcamApp.setVideoItem(videoItem, rootObject);
    } else {
        qWarning() << "Could not find videoItem";
    }

    ret = app.exec();
  }

  gst_deinit();
  return ret;
}
