# CosmosEx Linux apps: ce_cores (C++) + webserver (Go) in one image.
#
# Build from the repository root:
#   docker build -f docker/Dockerfile -t ce-atari .
#
# Preferred run (bind-mounts settings + host filesystem, host network
# so UDP 7200 can receive LAN broadcasts and reply unicast on 7201):
#   docker compose -f docker/docker-compose.yml up --build
#
# Equivalent docker run:
#   docker run --rm --network host \
#     -v /opt/ce/settings:/opt/ce/settings \
#     -v /:/mnt/hostdrive:rslave \
#     -v /dev/input:/dev/input \
#     --device-cgroup-rule='c 13:* rwm' \
#     -e CE_WEB_USER=ce -e CE_WEB_PASSWORD=ce \
#     ce-atari
#
# Layout matches the defaults in /.env:
#   BIN_DIR=/opt/ce/bin   SETTINGS_DIR=/opt/ce/settings
#   DATA_DIR=/tmp/ce/data LOG_DIR=/tmp/ce/log  PID_DIR=/tmp/ce/pid
# Host root is mounted at /mnt/hostdrive (see docker-compose.yml).

# ---------------------------------------------------------------------------
# Stage 1: C++ cores
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS cpp-builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY libdospath/ /src/libdospath/
COPY ce_cores/ /src/ce_cores/

# makefile copies libdospath.a into ce_cores/lib/
RUN mkdir -p /src/ce_cores/lib /src/ce_cores/obj \
    && make -C /src/ce_cores -j"$(nproc)"

# ---------------------------------------------------------------------------
# Stage 2: Go webserver (CGO required for PAM)
# ---------------------------------------------------------------------------
FROM golang:bookworm AS go-builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends libpam0g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src/webserver

COPY webserver/go.mod webserver/go.sum ./
RUN go mod download

COPY webserver/ ./
RUN CGO_ENABLED=1 go build -o /out/webserver .

# ---------------------------------------------------------------------------
# Stage 3: runtime image
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libstdc++6 \
        libpam0g \
        libpam-modules \
        libpam-runtime \
        passwd \
        tini \
        tzdata \
    && rm -rf /var/lib/apt/lists/*

# Defaults from .env — used at image build time for COPY destinations.
ENV CE_DIR=/opt/ce \
    BIN_DIR=/opt/ce/bin \
    SETTINGS_DIR=/opt/ce/settings \
    DATA_DIR=/tmp/ce/data \
    LOG_DIR=/tmp/ce/log \
    PID_DIR=/tmp/ce/pid \
    CONFIG_DRIVE_PATH=/tmp/ce/data/configdrive \
    HOSTDRIVE=/mnt/hostdrive \
    CE_WEB_USER=ce \
    CE_WEB_PASSWORD=ce

RUN mkdir -p "$BIN_DIR" "$SETTINGS_DIR" "$DATA_DIR" "$LOG_DIR" "$PID_DIR" "$HOSTDRIVE"

# Binaries
COPY --from=cpp-builder /src/ce_cores/ce_cores.elf /opt/ce/bin/ce_cores.elf
COPY --from=go-builder /out/webserver /opt/ce/bin/webserver

# .env defaults (repo-root file; both apps load ./ .env from BIN_DIR)
COPY .env /opt/ce/bin/.env

# Webserver static files (served as ./static from BIN_DIR)
COPY webserver/static/ /opt/ce/bin/static/

# ce_cores binary assets (logo, floppy images, config drive)
COPY ce_cores/ce_logo.bin /opt/ce/bin/ce_logo.bin
COPY ce_cores/core/ce_conf.st /opt/ce/bin/ce_conf.st
COPY ce_cores/core/fdd_test.st /opt/ce/bin/fdd_test.st
COPY ce_cores/core/ce_conf.zip /opt/ce/bin/ce_conf.zip
COPY ce_cores/configdrive/ /opt/ce/bin/configdrive/
COPY ce_cores/core/configdrive/ /opt/ce/bin/configdrive/

# Helper scripts used on the host install; handy inside the container too
COPY shellscripts/ce_start.sh shellscripts/ce_stop.sh /opt/ce/bin/
COPY docker/entrypoint.sh /opt/ce/bin/entrypoint.sh

RUN sed -i 's/\r$//' /opt/ce/bin/entrypoint.sh /opt/ce/bin/ce_start.sh \
        /opt/ce/bin/ce_stop.sh /opt/ce/bin/.env \
    && chmod +x /opt/ce/bin/ce_cores.elf /opt/ce/bin/webserver \
        /opt/ce/bin/ce_start.sh /opt/ce/bin/ce_stop.sh /opt/ce/bin/entrypoint.sh

WORKDIR /opt/ce/bin

EXPOSE 8080
EXPOSE 7200/udp
EXPOSE 7300 7301 7302

ENTRYPOINT ["/usr/bin/tini", "--", "/opt/ce/bin/entrypoint.sh"]
