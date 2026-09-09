To build docker image (from the repo root):
docker build -f docker/Dockerfile -t ce-atari .

Preferred run (persists settings, exposes the host filesystem, host network
for UDP discovery broadcasts):
docker compose -f docker/docker-compose.yml up --build

Equivalent docker run:
docker run --rm --network host \
  -v /opt/ce/settings:/opt/ce/settings \
  -v /:/mnt/hostdrive:rslave \
  -v /dev/input:/dev/input \
  --device-cgroup-rule='c 13:* rwm' \
  -e CE_WEB_USER=ce -e CE_WEB_PASSWORD=ce \
  ce-atari

Settings live on the host at /opt/ce/settings (same as SETTINGS_DIR in .env).
Override the host path with CE_SETTINGS_HOST=/your/path.

The host filesystem is mounted read-write at /mnt/hostdrive. Point GEM /
translated drive paths at /mnt/hostdrive/... in the web UI (for example
/mnt/hostdrive/home/user).

Discovery uses host networking (Linux). Devices broadcast UDP to port 7200;
the server unicasts the reply to the device on port 7201. Bridge port mapping
(-p 7200:7200/udp) does not receive LAN broadcasts. 8080 and 7300-7302 are
also bound directly on the host.

USB mice, keyboards and gamepads: IKBD lists /dev/input/by-path and opens
/dev/input/event* / js*. Host /dev/input is bind-mounted at the same path.
Char major 13 is allowed so newly plugged devices can be opened (hotplug
plus inotify on /dev/input and /dev/input/by-path).
