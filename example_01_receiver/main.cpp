#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include <iostream>
#include <gst/gstmacos.h>

#include "gstutil/auto-gst-object.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "util/argument-parser.hpp"
#include "examplsubdir/helper.hpp"

// Ensure static plugins (if using gstreamer-full) are registered
extern "C" void gst_init_static_plugins(void);


namespace {
declare_autoptr(GMainLoop, GMainLoop, g_main_loop_unref);
declare_autoptr(GstMessage, GstMessage, gst_message_unref);
declare_autoptr(GString, gchar, g_free);

// callbacks
struct Context {
    GstElement* pipeline;
    GstElement* videotestsrc;
};

auto jitsibin_pad_added_handler(GstElement* const /*jitsibin*/, GstPad* const pad, gpointer const data) -> void {
    auto& self = *static_cast<Context*>(data);

    const auto name_g = AutoGString(gst_object_get_name(GST_OBJECT(pad)));
    const auto name   = std::string_view(name_g.get());
    PRINT("pad added name={}", name);

    unwrap(pad_name, parse_jitsibin_pad_name(name));

    auto decoder = std::string();
    // TODO: handle all codec type
    if(pad_name.codec == "OPUS") {
        decoder = "TODO";
    } else if(pad_name.codec == "H264") {
        decoder = "openh264dec";
    } else if(pad_name.codec == "VP8") {
        decoder = "avdec_vp8";
    } else if(pad_name.codec == "VP9") {
        decoder = "TODO";
    } else if(pad_name.codec == "AV1") {
        decoder = "av1dec";
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

    // for video
    PRINT("Creating decoder pipeline for {}", decoder);
    unwrap_mut(dec, add_new_element_to_pipeine(self.pipeline, decoder.data()));
    PRINT("Created decoder");
    unwrap_mut(videoconvert, add_new_element_to_pipeine(self.pipeline, "videoconvert"));
    PRINT("Created videoconvert");
    unwrap_mut(capsfilter_out, add_new_element_to_pipeine(self.pipeline, "capsfilter"));
    PRINT("Created output capsfilter");
    
    // Use osxvideosink with proper settings for macOS
    unwrap_mut(videosink, add_new_element_to_pipeine(self.pipeline, "osxvideosink"));
    g_object_set(&videosink,
                 "async", FALSE,
                 "sync", FALSE,
                 NULL);
    PRINT("Created videosink (osxvideosink)");
    
    g_object_set(&dec,
                 "automatic-request-sync-points", TRUE,
                 "automatic-request-sync-point-flags", GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT,
                 NULL);
    PRINT("Configured decoder");

    // Link source pad to decoder (with parser for AV1/H264 as needed)
    if(pad_name.codec == "AV1") {
        unwrap_mut(parser, add_new_element_to_pipeine(self.pipeline, "av1parse"));
        PRINT("Created av1parse");
        const auto parser_sink_pad = AutoGstObject(gst_element_get_static_pad(&parser, "sink"));
        ensure(parser_sink_pad.get() != NULL);
        PRINT("Linking jitsibin pad to av1parse...");
        ensure(gst_pad_link(pad, GST_PAD(parser_sink_pad.get())) == GST_PAD_LINK_OK);
        PRINT("Linked jitsibin -> av1parse");
        ensure(gst_element_link_pads(&parser, NULL, &dec, NULL) == TRUE);
        PRINT("Linked av1parse -> decoder");
    } else if(pad_name.codec == "H264") {
        unwrap_mut(parser, add_new_element_to_pipeine(self.pipeline, "h264parse"));
        PRINT("Created h264parse");
        const auto parser_sink_pad = AutoGstObject(gst_element_get_static_pad(&parser, "sink"));
        ensure(parser_sink_pad.get() != NULL);
        PRINT("Linking jitsibin pad to h264parse...");
        ensure(gst_pad_link(pad, GST_PAD(parser_sink_pad.get())) == GST_PAD_LINK_OK);
        PRINT("Linked jitsibin -> h264parse");
        ensure(gst_element_link_pads(&parser, NULL, &dec, NULL) == TRUE);
        PRINT("Linked h264parse -> decoder");
    } else {
        const auto dec_sink_pad = AutoGstObject(gst_element_get_static_pad(&dec, "sink"));
        ensure(dec_sink_pad.get() != NULL);
        PRINT("Got decoder sink pad");
        PRINT("Linking jitsibin pad to decoder...");
        ensure(gst_pad_link(pad, GST_PAD(dec_sink_pad.get())) == GST_PAD_LINK_OK);
        PRINT("Linked jitsibin -> decoder");
    }
    
    ensure(gst_element_link_pads(&dec, NULL, &videoconvert, NULL) == TRUE);
    PRINT("Linked decoder -> videoconvert");
    
    // Constrain to formats osxvideosink handles well
    {
        auto* caps = gst_caps_from_string("video/x-raw,format=(string){ UYVY }");
        g_object_set(&capsfilter_out, "caps", caps, NULL);
        gst_caps_unref(caps);
    }
    ensure(gst_element_link_pads(&videoconvert, NULL, &capsfilter_out, NULL) == TRUE);
    PRINT("Linked videoconvert -> capsfilter_out");
    ensure(gst_element_link_pads(&capsfilter_out, NULL, &videosink, NULL) == TRUE);
    PRINT("Linked capsfilter_out -> videosink");
    
    PRINT("Syncing states...");
    // Sync decoder first (it needs to receive data)
    ensure(gst_element_sync_state_with_parent(&dec) == TRUE);
    PRINT("Synced decoder");
    ensure(gst_element_sync_state_with_parent(&videoconvert) == TRUE);
    PRINT("Synced videoconvert");
    ensure(gst_element_sync_state_with_parent(&capsfilter_out) == TRUE);
    PRINT("Synced capsfilter_out");
    // Sync videosink last with async state change handling
    const auto ret = gst_element_set_state(&videosink, GST_STATE_PLAYING);
    ensure(ret != GST_STATE_CHANGE_FAILURE);
    PRINT("Started videosink (async={}, state={})",
          ret == GST_STATE_CHANGE_ASYNC ? "true" : "false",
          ret == GST_STATE_CHANGE_SUCCESS ? "playing" : "pending");
    
    PRINT("Successfully added {} decoder pipeline!", decoder);
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
    const auto& self = *static_cast<Context*>(data);
    const auto  bus  = AutoGstObject(gst_element_get_bus(self.pipeline));
    ensure(gst_bus_post(bus.get(), gst_message_new_eos(NULL)) == TRUE);
}
} // namespace

static int app_main(int argc, char** argv, gpointer /*user_data*/) {
    const char* host = nullptr;
    const char* room = nullptr;
    {
        auto help   = false;
        auto parser = args::Parser<>();
        parser.arg(&host, "HOST", "server domain");
        parser.arg(&room, "ROOM", "room name");
        parser.kwflag(&help, {"-h", "--help"}, "print this help message", {.no_error_check = true});
        if(!parser.parse(argc, argv) || help) {
            std::cout << "usage: example " << parser.get_help() << std::endl;
            return 0;
        }
    }

    gst_init(NULL, NULL);
    gst_init_static_plugins();

    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline.get() != NULL);

    auto context = Context{
        .pipeline = pipeline.get(),
    };

    /*
     * videotestsrc -> tee -> waylandsink
     *                     -> videoconvert -> x264enc -> jitisbin
     * audiotestsrc ->                        opusenc ->
     */

    unwrap_mut(videotestsrc, add_new_element_to_pipeine(pipeline.get(), "videotestsrc"));
    unwrap_mut(videoconvert, add_new_element_to_pipeine(pipeline.get(), "videoconvert"));
    unwrap_mut(openh264enc, add_new_element_to_pipeine(pipeline.get(), "openh264enc"));
    unwrap_mut(h264parse, add_new_element_to_pipeine(pipeline.get(), "h264parse"));
    unwrap_mut(audiotestsrc, add_new_element_to_pipeine(pipeline.get(), "audiotestsrc"));
    unwrap_mut(opusenc, add_new_element_to_pipeine(pipeline.get(), "opusenc"));
    unwrap_mut(jitsibin, add_new_element_to_pipeine(pipeline.get(), "jitsibin"));
    g_signal_connect(&jitsibin, "pad-added", G_CALLBACK(jitsibin_pad_added_handler), &context);
    g_signal_connect(&jitsibin, "pad-removed", G_CALLBACK(jitsibin_pad_removed_handler), &context);
    g_signal_connect(&jitsibin, "participant-joined", G_CALLBACK(jitsibin_participant_joined_handler), &context);
    g_signal_connect(&jitsibin, "participant-left", G_CALLBACK(jitsibin_participant_left_handler), &context);
    g_signal_connect(&jitsibin, "mute-state-changed", G_CALLBACK(jitsibin_mute_state_changed_handler), &context);
    g_signal_connect(&jitsibin, "finished", G_CALLBACK(jitsibin_finished_handler), &context);

    g_object_set(&videotestsrc,
                 "is-live", TRUE,
                 NULL);
    g_object_set(&audiotestsrc,
                 "is-live", TRUE,
                 "wave", 8,
                 NULL);
    g_object_set(&openh264enc,
                 "gop-size", 30,
                 NULL);
    g_object_set(&jitsibin,
                 "server", host,
                 "room", room,
                 "nick", "gstjitsimeet-example",
                 "video-codec", 1,
                 "receive-limit", 3,
                 "force-play", TRUE,
                 "insecure", TRUE,
                 NULL);

    ensure(gst_element_link_pads(&videotestsrc, NULL, &videoconvert, NULL) == TRUE);
    ensure(gst_element_link_pads(&videoconvert, NULL, &openh264enc, NULL) == TRUE);
    ensure(gst_element_link_pads(&openh264enc, NULL, &h264parse, NULL) == TRUE);
    ensure(gst_element_link_pads(&h264parse, NULL, &jitsibin, "video_sink") == TRUE);
    ensure(gst_element_link_pads(&audiotestsrc, NULL, &opusenc, NULL) == TRUE);
    ensure(gst_element_link_pads(&opusenc, NULL, &jitsibin, "audio_sink") == TRUE);

    ensure(run_pipeline(pipeline.get()));

    return 0;
}

auto main(int argc, char** argv) -> int {
    return gst_macos_main(app_main, argc, argv, NULL);
}


