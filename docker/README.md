To build docker image (from the repo root):
docker build -f docker/Dockerfile -t ce-atari .

Preferred run (persists settings, exposes the host filesystem, host network
for UDP discovery broadcasts):
docker compose -f docker/docker-compose.yml up --build

Interactive run - ce_cores and webserver output in the same console, each
line prefixed with [cores] or [web]:
docker compose -f docker/docker-compose.yml run --rm ce
The same output is in docker logs when running with up -d.

Equivalent docker run:
docker run --rm --network host \
  -v /opt/ce/settings:/opt/ce/settings \
  -v /:/mnt/hostdrive:rslave \
  -v /dev:/dev \
  --device-cgroup-rule='c 13:* rwm' \
  --device-cgroup-rule='b 8:* rwm' \
  ce-atari

Settings live on the host at /opt/ce/settings (same as SETTINGS_DIR in .env).
Override the host path with CE_SETTINGS_HOST=/your/path.

Web UI password: the default is 'ce'. After the first login you have to set a
new password; its bcrypt hash is stored in SETTINGS_DIR/web_password_hash.
Forgot the password? Delete that file on the host and log in with 'ce' again.

HTTPS: the web UI is served over HTTPS on port 8080; plain http:// on the same
port redirects to https://. On first start a local CA (valid 20 years) is
created in SETTINGS_DIR/tls, and the server certificate (825 days) is issued
from it for the hostname and current IPs. The webserver checks it hourly and
re-issues it before it expires or when the IPs change, always from the same CA.
To get rid of browser warnings, download the CA from https://<box>:8080/ca.crt
(link on the login page) and install it once as a trusted root on your PC.
The CA is limited to local names and private IP ranges.

The host filesystem is mounted read-write at /mnt/hostdrive. Point GEM /
translated drive paths at /mnt/hostdrive/... in the web UI (for example
/mnt/hostdrive/home/user).

Discovery uses host networking (Linux). Devices broadcast UDP to port 7200;
the server unicasts the reply to the device on port 7201. Bridge port mapping
(-p 7200:7200/udp) does not receive LAN broadcasts. 8080 and 7300-7302 are
also bound directly on the host.

USB mice, keyboards and gamepads: IKBD lists /dev/input/by-path and opens
/dev/input/event* / js*. Host /dev is bind-mounted at the same path.
Char major 13 is allowed so newly plugged devices can be opened (hotplug
plus inotify on /dev/input and /dev/input/by-path).

USB raw disks for HDD emulation (/dev/sd*): block major 8 is allowed.
WARNING: this includes any SATA/USB system disk of the host.
