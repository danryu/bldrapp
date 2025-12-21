# Jitsi Meet Docker Setup for qnujitsi Testing

This directory contains a Docker Compose configuration for running a self-hosted Jitsi Meet server for automated testing of the qnujitsi application.

## Prerequisites

- Docker Engine 20.10 or later
- Docker Compose 1.29 or later (or docker-compose plugin)
- At least 2GB of available RAM
- Ports 8000, 30443, 10000/udp, and 4443 must be available

## Quick Start

### 1. Configure Environment

Edit [.env](.env:1) if needed, especially:

```bash
# On Linux, set this to your machine's local IP
DOCKER_HOST_ADDRESS=192.168.1.100  # Replace with your IP

# On macOS/Windows, use the default:
DOCKER_HOST_ADDRESS=host.docker.internal
```

To find your IP address:
- macOS/Linux: `ifconfig | grep "inet "`
- Windows: `ipconfig`

### 2. Start Jitsi Meet Stack

```bash
docker-compose up -d
```

This will pull the required Docker images (first run only) and start:
- Jitsi Meet web interface (nginx)
- Prosody XMPP server
- Jicofo (conference focus/control)
- JVB (Jitsi Videobridge for media routing)

### 3. Wait for Services to Initialize

Give the services ~10-15 seconds to fully start:

```bash
sleep 15
```

### 4. Verify Setup

Check that the web interface is accessible:

```bash
curl -I http://localhost:8000
```

You should see a `200 OK` response.

Or open in your browser: http://localhost:8000

### 5. Stop Services

When done testing:

```bash
docker-compose down
```

To also remove volumes (config, logs):

```bash
docker-compose down -v
```

## Service Details

### Web Interface
- HTTP: http://localhost:8000
- HTTPS: https://localhost:30443 (self-signed certificate)

### Exposed Ports
- `8000`: HTTP web interface
- `30443`: HTTPS web interface
- `10000/udp`: JVB media port (WebRTC)
- `4443`: JVB TCP fallback

### Service Configuration

All services are configured for **testing** with:
- **No authentication** (`ENABLE_AUTH=0`)
- **Guests enabled** (`ENABLE_GUESTS=1`)
- **No HTTPS redirect** (easier for automated testing)
- **Permissive settings** (auto-join, no waiting rooms)

**WARNING**: Do NOT use this configuration in production. It has no security.

## Logs and Debugging

### View all logs
```bash
docker-compose logs -f
```

### View specific service logs
```bash
docker-compose logs -f web
docker-compose logs -f jvb
docker-compose logs -f prosody
docker-compose logs -f jicofo
```

### Check service status
```bash
docker-compose ps
```

All services should show `Up` status.

## Troubleshooting

### Services won't start
1. Check if ports are already in use:
   ```bash
   lsof -i :8000
   lsof -i :30443
   lsof -i :10000
   ```
2. Stop conflicting services or change ports in [docker-compose.yml](docker-compose.yml:1)

### Can't connect from qnujitsi
1. Verify Jitsi is accessible: `curl http://localhost:8000`
2. Check JVB logs: `docker-compose logs jvb`
3. On Linux, ensure `DOCKER_HOST_ADDRESS` in [.env](.env:1) is your actual IP, not `host.docker.internal`

### Media (audio/video) not working
1. Ensure UDP port 10000 is open: `sudo ufw allow 10000/udp` (Linux)
2. Check JVB harvester configuration in logs
3. Verify `DOCKER_HOST_ADDRESS` is correctly set

### Cleanup completely
```bash
docker-compose down -v
rm -rf config/ web/ transcripts/
```

## Testing with qnujitsi

Once Jitsi is running, configure qnujitsi to connect:

1. Host: `localhost:8000` (or `localhost:30443` for HTTPS)
2. Room: any name (e.g., `testroom`)
3. Click Connect

## Integration with Test Suite

The test orchestration scripts ([../scripts/run_tests.sh](../scripts/run_tests.sh:1)) handle:
- Starting this stack automatically
- Waiting for services to be ready
- Running tests
- Cleaning up afterward

Manual usage is primarily for debugging.

## Updating Jitsi Version

To update to a newer Jitsi stable version:

1. Edit [docker-compose.yml](docker-compose.yml:1)
2. Change all image tags from `stable-9258` to desired version
3. Pull new images: `docker-compose pull`
4. Restart: `docker-compose down && docker-compose up -d`

See [Jitsi Docker releases](https://github.com/jitsi/docker-jitsi-meet/releases) for version tags.
