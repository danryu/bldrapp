# Multi-stage build based on upstream gst-meet Dockerfile
# Source: https://github.com/avstack/gst-meet/blob/master/Dockerfile

FROM docker.io/library/alpine:3.20.0 AS builder

# Install build dependencies
RUN apk --no-cache --update upgrade --ignore alpine-baselayout \
 && apk --no-cache add \
    build-base \
    gstreamer-dev \
    gst-plugins-base-dev \
    libnice-dev \
    openssl-dev \
    cargo \
    cmake \
    clang16-libclang \
    rust-bindgen \
    git

# Clone and build gst-meet with tls-insecure feature
# Apply fixes to update tls.rs for rustls 0.22 API (current upstream)
WORKDIR /build
COPY fix-tls.sh /tmp/fix-tls.sh
RUN chmod +x /tmp/fix-tls.sh \
    && git clone https://github.com/avstack/gst-meet.git \
    && /tmp/fix-tls.sh \
    && cd gst-meet \
    && cargo build --release --features tls-insecure -p gst-meet

# Runtime stage
FROM docker.io/library/alpine:3.20.0

# Install runtime dependencies
RUN apk --update --no-cache upgrade --ignore alpine-baselayout \
 && apk --no-cache add \
    openssl \
    gstreamer \
    gst-plugins-good \
    gst-plugins-bad \
    gst-plugins-ugly \
    gst-libav \
    libnice \
    libnice-gstreamer \
    bash

# Copy gst-meet binary from builder
COPY --from=builder /build/gst-meet/target/release/gst-meet /usr/local/bin/

# Copy our custom entry point script for test configuration
COPY run_participant.sh /usr/local/bin/run_participant.sh
RUN chmod +x /usr/local/bin/run_participant.sh

# Set working directory
WORKDIR /app

# Entry point (uses our wrapper script for easy configuration)
ENTRYPOINT ["/usr/local/bin/run_participant.sh"]
