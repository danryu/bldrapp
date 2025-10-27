#include <gst/gst.h>
#include <gst/gstmacos.h>
#include <iostream>

// This is your "real" main, to be run inside the Cocoa NSApplication context
int gst_main_func(int argc, char *argv[], gpointer user_data) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_parse_launch("videotestsrc ! glimagesink", NULL);
    if (!pipeline) {
        std::cerr << "Failed to create pipeline\n";
        return -1;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != nullptr) {
        GError *err = nullptr;
        gchar *debug_info = nullptr;

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            gst_message_parse_error(msg, &err, &debug_info);
            std::cerr << "Error received: " << err->message << std::endl;
            g_error_free(err);
            g_free(debug_info);
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            std::cout << "End of stream" << std::endl;
        }

        gst_message_unref(msg);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    return 0;
}

int main(int argc, char *argv[]) {
    // ✅ Launch GStreamer logic inside a proper NSApplication environment
    return gst_macos_main(gst_main_func, argc, argv, nullptr);
}
