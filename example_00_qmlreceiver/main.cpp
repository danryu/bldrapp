#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QMetaObject>
#include <QRunnable>
#include <QUrl>

#include <atomic>
#include <thread>
#include "gstutil/auto-gst-object.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "util/argument-parser.hpp"
#include "examplsubdir/helper.hpp"

// Forward declare static plugin initialization function
extern "C" void gst_init_static_plugins(void);

// Register static resources from libgstqml6 (shaders for qml6glsink)
extern int qInitResources_resources();

class SetPlaying : public QRunnable {
public:
    SetPlaying(GstElement* pipeline);
    ~SetPlaying();
    void run() override;

private:
    GstElement* pipeline_;
};

SetPlaying::SetPlaying(GstElement* pipeline) {
    this->pipeline_ = pipeline ? static_cast<GstElement*>(gst_object_ref(pipeline)) : nullptr;
}

SetPlaying::~SetPlaying() {
    if(this->pipeline_) {
        gst_object_unref(this->pipeline_);
    }
}

void SetPlaying::run() {
    if(this->pipeline_) {
        gst_element_set_state(this->pipeline_, GST_STATE_PLAYING);
    }
}

namespace {
declare_autoptr(GMainLoop, GMainLoop, g_main_loop_unref);
declare_autoptr(GstMessage, GstMessage, gst_message_unref);
declare_autoptr(GString, gchar, g_free);

struct RemoteSlot {
    GstElement* videoconvert = nullptr;
    GstElement* queue        = nullptr;
    GstElement* glupload     = nullptr;
    GstElement* glcolorconvert = nullptr;
    GstElement* sink         = nullptr;
    bool        connected    = false;
};

// callbacks
struct Context {
    GstElement* pipeline = nullptr;
    RemoteSlot  remote;
};

auto jitsibin_pad_added_handler(GstElement* const /*jitsibin*/, GstPad* const pad, gpointer const data) -> void {
    auto& self = *std::bit_cast<Context*>(data);

    const auto name_g = AutoGString(gst_object_get_name(GST_OBJECT(pad)));
    const auto name   = std::string_view(name_g.get());
    PRINT("pad added name={}", name);

    unwrap(pad_name, parse_jitsibin_pad_name(name));

    // Skip audio pads - we only handle video
    if(pad_name.codec == "OPUS") {
        PRINT("skipping audio pad {}", name);
        return;
    }

    auto decoder = std::string();
    // TODO: handle all codec type
    if(pad_name.codec == "H264") {
        decoder = "vtdec";
    } else if(pad_name.codec == "VP8") {
        decoder = "avdec_vp8";
    } else if(pad_name.codec == "VP9" || pad_name.codec == "AV1") {
        decoder = "TODO";
    } else {
        PRINT("unsupported codec {}", pad_name.codec);
        decoder = "fakesink";
        return;
    }

    if(decoder == "TODO") {
        unwrap_mut(fakesink, add_new_element_to_pipeine(self.pipeline, "fakesink"));
        const auto fakesink_sink_pad = AutoGstObject(gst_element_get_static_pad(&fakesink, "sink"));
        ensure(fakesink_sink_pad.get() != NULL);
        ensure(gst_pad_link(pad, fakesink_sink_pad.get()) == GST_PAD_LINK_OK);
        ensure(gst_element_sync_state_with_parent(&fakesink) == TRUE);
        return;
    }

    auto& remote_slot = self.remote;
    ensure(remote_slot.videoconvert != nullptr);
    ensure(remote_slot.sink != nullptr);

    if(remote_slot.connected) {
        PRINT("remote slot already connected; ignoring pad {}", name);
        return;
    }

    unwrap_mut(h264parse, add_new_element_to_pipeine(self.pipeline, "h264parse"));
    unwrap_mut(dec, add_new_element_to_pipeine(self.pipeline, decoder.data()));
    g_object_set(&dec,
                 "automatic-request-sync-points", TRUE,
                 "automatic-request-sync-point-flags", GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT,
                 NULL);

    const auto h264parse_sink_pad = AutoGstObject(gst_element_get_static_pad(&h264parse, "sink"));
    ensure(h264parse_sink_pad.get() != NULL);
    ensure(gst_pad_link(pad, GST_PAD(h264parse_sink_pad.get())) == GST_PAD_LINK_OK);
    ensure(gst_element_link_pads(&h264parse, NULL, &dec, NULL) == TRUE);
    ensure(gst_element_link_pads(&dec, NULL, remote_slot.videoconvert, NULL) == TRUE);
    ensure(gst_element_sync_state_with_parent(remote_slot.videoconvert) == TRUE);
    if(remote_slot.queue != nullptr) {
        ensure(gst_element_sync_state_with_parent(remote_slot.queue) == TRUE);
    }
    if(remote_slot.glupload != nullptr) {
        ensure(gst_element_sync_state_with_parent(remote_slot.glupload) == TRUE);
    }
    if(remote_slot.glcolorconvert != nullptr) {
        ensure(gst_element_sync_state_with_parent(remote_slot.glcolorconvert) == TRUE);
    }
    ensure(gst_element_sync_state_with_parent(remote_slot.sink) == TRUE);
    ensure(gst_element_sync_state_with_parent(&dec) == TRUE);
    ensure(gst_element_sync_state_with_parent(&h264parse) == TRUE);
    remote_slot.connected = true;
    PRINT("added h264 decoder feeding qml6glsink");
}

auto jitsibin_pad_removed_handler(GstElement* const /*jitisbin*/, GstPad* const pad, gpointer const /*data*/) -> void {
    const auto name_g = AutoGString(gst_object_get_name(GST_OBJECT(pad)));
    const auto name   = std::string_view(name_g.get());
    PRINT("pad removed name={}", name);
}

auto jitsibin_participant_joined_handler(GstElement* const /*jitisbin*/, const gchar* const participant_id, const gchar* const nick, gpointer const /*data*/) -> void {
    PRINT("participant joined id={} nick={}", participant_id, nick);
}

auto jitsibin_participant_left_handler(GstElement* const /*jitisbin*/, const gchar* const participant_id, const gchar* const nick, gpointer const /*data*/) -> void {
    PRINT("participant left id={} nick={}", participant_id, nick);
}

auto jitsibin_mute_state_changed_handler(GstElement* const /*jitisbin*/, const gchar* const participant_id, const gboolean is_audio, const gboolean new_muted, gpointer const /*data*/) -> void {
    PRINT("mute state changed id={} {}={}", participant_id, is_audio ? "audio" : "video", new_muted);
}

auto jitsibin_finished_handler(GstElement* const /*jitisbin*/, const gboolean success, gpointer const data) -> void {
    PRINT("finished success={}", success);
    const auto& self = *std::bit_cast<Context*>(data);
    const auto  bus  = AutoGstObject(gst_element_get_bus(self.pipeline));
    ensure(gst_bus_post(bus.get(), gst_message_new_eos(NULL)) == TRUE);
}
} // namespace

static int app_main(int argc, char** argv) {
    const char* host = nullptr;
    const char* room = nullptr;
    {
        auto help   = false;
        auto parser = args::Parser<>();
        parser.arg(&host, "HOST", "server domain");
        parser.arg(&room, "ROOM", "room name");
        parser.kwflag(&help, {"-h", "--help"}, "print this help message", {.no_error_check = true});
        if(!parser.parse(argc, argv) || help) {
            std::println("usage: example {}", parser.get_help());
            return 0;
        }
    }

    gst_init(&argc, &argv);
    gst_init_static_plugins();

    QGuiApplication app(argc, argv);
    qInitResources_resources();
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // Process events to ensure NSApplication is initialized on macOS
    QCoreApplication::processEvents();

    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline.get() != NULL);

    auto context = Context{
        .pipeline = pipeline.get(),
    };

    /*
     * videotestsrc -> tee -> osxvideosink
     *                     -> videoconvert -> vtenc_h264 -> jitisbin
     * audiotestsrc ->                        opusenc ->
     */

    unwrap_mut(videotestsrc, add_new_element_to_pipeine(pipeline.get(), "videotestsrc"));
    unwrap_mut(tee, add_new_element_to_pipeine(pipeline.get(), "tee"));
    unwrap_mut(osxvideosink, add_new_element_to_pipeine(pipeline.get(), "osxvideosink"));
    unwrap_mut(videoconvert, add_new_element_to_pipeine(pipeline.get(), "videoconvert"));
    unwrap_mut(vtenc_h264, add_new_element_to_pipeine(pipeline.get(), "vtenc_h264"));
    unwrap_mut(audiotestsrc, add_new_element_to_pipeine(pipeline.get(), "audiotestsrc"));
    unwrap_mut(opusenc, add_new_element_to_pipeine(pipeline.get(), "opusenc"));
    unwrap_mut(jitsibin, add_new_element_to_pipeine(pipeline.get(), "jitsibin"));
    unwrap_mut(remote_videoconvert, add_new_element_to_pipeine(pipeline.get(), "videoconvert"));
    unwrap_mut(remote_queue, add_new_element_to_pipeine(pipeline.get(), "queue"));
    unwrap_mut(remote_glupload, add_new_element_to_pipeine(pipeline.get(), "glupload"));
    unwrap_mut(remote_glcolorconvert, add_new_element_to_pipeine(pipeline.get(), "glcolorconvert"));
    /* the plugin must be loaded before loading the qml file to register the
     * GstGLVideoItem qml item */
    unwrap_mut(remote_sink, add_new_element_to_pipeine(pipeline.get(), "qml6glsink"));

    g_object_set(&remote_queue,
                 "max-size-buffers", 5,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 "leaky", 2,
                 NULL);

    context.remote = RemoteSlot{
        .videoconvert   = &remote_videoconvert,
        .queue          = &remote_queue,
        .glupload       = &remote_glupload,
        .glcolorconvert = &remote_glcolorconvert,
        .sink           = &remote_sink,
        .connected      = false,
    };

    g_signal_connect(&jitsibin, "pad-added", G_CALLBACK(jitsibin_pad_added_handler), &context);
    g_signal_connect(&jitsibin, "pad-removed", G_CALLBACK(jitsibin_pad_removed_handler), &context);
    g_signal_connect(&jitsibin, "participant-joined", G_CALLBACK(jitsibin_participant_joined_handler), &context);
    g_signal_connect(&jitsibin, "participant-left", G_CALLBACK(jitsibin_participant_left_handler), &context);
    g_signal_connect(&jitsibin, "mute-state-changed", G_CALLBACK(jitsibin_mute_state_changed_handler), &context);
    g_signal_connect(&jitsibin, "finished", G_CALLBACK(jitsibin_finished_handler), &context);

    g_object_set(&osxvideosink,
                 "async", FALSE,
                 NULL);
    g_object_set(&videotestsrc,
                 "is-live", TRUE,
                 NULL);
    g_object_set(&audiotestsrc,
                 "is-live", TRUE,
                 "wave", 8,
                 NULL);
    g_object_set(&vtenc_h264,
                 "realtime", TRUE,
                //  "tune", 0x04,
                 NULL);
    g_object_set(&jitsibin,
                 "server", host,
                 "room", room,
                 "nick", "gstjitsimeet-example",
                 "receive-limit", 3,
                 "force-play", TRUE,
                 "receive-max-height", 720,
                 "insecure", TRUE,
                 NULL);

    ensure(gst_element_link_pads(&videotestsrc, NULL, &tee, NULL) == TRUE);
    ensure(gst_element_link_pads(&tee, NULL, &osxvideosink, NULL) == TRUE);
    ensure(gst_element_link_pads(&tee, NULL, &videoconvert, NULL) == TRUE);
    ensure(gst_element_link_pads(&videoconvert, NULL, &vtenc_h264, NULL) == TRUE);
    ensure(gst_element_link_pads(&vtenc_h264, NULL, &jitsibin, "video_sink") == TRUE);
    ensure(gst_element_link_pads(&audiotestsrc, NULL, &opusenc, NULL) == TRUE);
    ensure(gst_element_link_pads(&opusenc, NULL, &jitsibin, "audio_sink") == TRUE);
    ensure(gst_element_link_many(&remote_videoconvert, &remote_queue, &remote_glupload, &remote_glcolorconvert, &remote_sink, NULL) == TRUE);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    ensure(!engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    ensure(window != nullptr);
    window->show();
    auto* video_item = window->findChild<QQuickItem*>("videoItem");
    ensure(video_item != nullptr);

    g_object_set(&remote_sink,
                 "widget", video_item,
                 "sync", TRUE,
                 "async", FALSE,
                 NULL);

    window->scheduleRenderJob(new SetPlaying(pipeline.get()),
                              QQuickWindow::BeforeSynchronizingStage);

    std::atomic<bool> stop_flag{false};
    auto* pipeline_ref = GST_ELEMENT_CAST(gst_object_ref(pipeline.get()));
    auto bus_thread    = std::thread([pipeline_ref, &stop_flag]() {
        const auto bus = AutoGstObject(gst_element_get_bus(pipeline_ref));
        if(bus.get() == NULL) {
            gst_object_unref(pipeline_ref);
            return;
        }

        while(!stop_flag.load()) {
            const auto msg = AutoGstMessage(gst_bus_timed_pop_filtered(bus.get(),
                                                                       250 * GST_MSECOND,
                                                                       GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)));
            if(!msg) {
                continue;
            }

            switch(GST_MESSAGE_TYPE(msg.get())) {
            case GST_MESSAGE_ERROR: {
                GError* err = nullptr;
                gchar*  dbg = nullptr;
                gst_message_parse_error(msg.get(), &err, &dbg);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err ? err->message : "unknown");
                g_printerr("Debugging information: %s\n", dbg ? dbg : "none");
                g_clear_error(&err);
                g_free(dbg);
            } break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;
            default:
                break;
            }

            if(auto* qt_app = QCoreApplication::instance()) {
                QMetaObject::invokeMethod(qt_app, &QCoreApplication::quit, Qt::QueuedConnection);
            }
            break;
        }

        gst_object_unref(pipeline_ref);
    });

    const int ret = app.exec();

    stop_flag.store(true);
    if(bus_thread.joinable()) {
        bus_thread.join();
    }
    gst_element_set_state(pipeline.get(), GST_STATE_NULL);

    return ret;
}

auto main(int argc, char** argv) -> int {
    return app_main(argc, argv);
}