from os import system
import urwid
import logging
from urwid_helpers import create_my_button, create_header_footer, create_edit, MyCheckBox, dialog
from utils import settings_load, on_cancel, back_to_main_menu, setting_get_bool, on_editline_changed, \
    on_checkbox_changed, setting_get_merged
import shared
import netifaces as ni
import dns.resolver
from IPy import IP
import subprocess

app_log = logging.getLogger()


def create_setting_row(label, what, value, col1w, col2w, reverse=False, setting_name=None, return_widget=False):
    wret = None     # widget to return

    if what == 'checkbox':      # for checkbox
        checked = setting_get_bool(setting_name)
        widget = MyCheckBox('', state=checked, on_state_change=on_net_checkbox_changed)
        wret = widget
        widget.setting_name = setting_name
        label = "   " + label
    elif what == 'edit':        # for edit line
        widget, _ = create_edit(setting_name, col2w, on_editline_changed)
        wret = widget
        label = "   " + label
    elif what == 'text':
        widget = urwid.Text(value)
        wret = widget
        if reverse:     # if should be reversed, apply attrmap
            widget = urwid.AttrMap(widget, 'reversed')
    else:                       # for title
        widget = urwid.Text('')

    # create label text
    text_label = urwid.Text(label)

    if reverse:         # if should be reversed, apply attrmap
        text_label = urwid.AttrMap(text_label, 'reversed')

    # put things into columns
    cols = urwid.Columns([
        ('fixed', col1w, text_label),
        ('fixed', col2w, widget)],
        dividechars=0)

    if return_widget:       # if should return also the individual widget
        return cols, wret

    return cols


def get_eth_iface(default=None):
    return get_iface_for('eth0', 'enp', default)


def get_wifi_iface(default=None):
    return get_iface_for('wlan0', 'wlp', default)


def get_iface_for(old_name, predictable_name, default=None):
    # go through interfaces, find ethernet
    ifaces = ni.interfaces()

    eth = default

    if old_name in ifaces:        # if eth0 is present, use it
        return old_name

    # try to look for predictable eth names
    for iface in ifaces:
        if iface.startswith(predictable_name):     # this iface name starts with the 'enp', use it
            eth = iface

    return eth


def get_gateway_for_iface(iface):
    gateways = ni.gateways()[ni.AF_INET]

    for gw in gateways:         # go through the found gateways
        gw_ip, gw_iface, gw_is_default = gw

        if gw_iface == iface:   # if this gateway is for our iface, return IP
            return gw_ip

    return ''                   # no gw found


def get_iface_use_dhcp(iface):
    result = subprocess.run(['ip', 'r'], stdout=subprocess.PIPE)        # run 'ip r'
    result = result.stdout.decode('utf-8')  # get output as string
    lines = result.split('\n')              # split whole result to lines

    for line in lines:          # go through the individual lines
        if iface not in line:   # this line is not for this iface, skip rest
            continue

        return 'dhcp' in line   # use dhcp, if dhcp in the line for this iface

    return True        # iface not found, just enable dhcp


def load_network_settings():
    # get hostname
    with open('/etc/hostname') as f:
        hostname = f.read()
        hostname = hostname.strip()
        shared.settings['HOSTNAME'] = hostname

    # get ethernet setting
    eth = get_eth_iface()

    if eth:     # got some iface name?
        ni_eth = ni.ifaddresses(eth)
        shared.settings['ETH_IP'] = ni_eth[ni.AF_INET][0]['addr']
        shared.settings['ETH_MASK'] = ni_eth[ni.AF_INET][0]['netmask']
        shared.settings['ETH_GW'] = get_gateway_for_iface(eth)
        shared.settings['ETH_USE_DHCP'] = get_iface_use_dhcp(eth)

    # find first DNS
    dns_resolver = dns.resolver.Resolver()
    shared.settings['DNS'] = dns_resolver.nameservers[0]


def network_create(button):
    settings_load()
    load_network_settings()

    header, footer = create_header_footer('Network settings')

    body = []
    body.append(urwid.Divider())

    col1w = 16
    col2w = 17

    # hostname and DNS
    cols = create_setting_row('Hostname', 'edit', '', col1w, col2w, False, 'HOSTNAME')
    body.append(cols)

    cols = create_setting_row('DNS', 'edit', '', col1w, col2w, False, 'DNS')
    body.append(cols)
    body.append(urwid.Divider())

    # ethernet settings
    cols = create_setting_row('Ethernet', 'title', '', col1w, col2w)
    body.append(cols)

    cols = create_setting_row('Use DHCP', 'checkbox', False, col1w, col2w, False, 'ETH_USE_DHCP')
    body.append(cols)

    cols = create_setting_row('IP address', 'edit', '', col1w, col2w, False, 'ETH_IP')
    body.append(cols)

    cols = create_setting_row('Mask', 'edit', '', col1w, col2w, False, 'ETH_MASK')
    body.append(cols)

    cols = create_setting_row('Gateway', 'edit', '', col1w, col2w, False, 'ETH_GW')
    body.append(cols)
    body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", network_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)


def on_net_checkbox_changed(widget, state):
    on_checkbox_changed(widget.setting_name, state)


config_file_dhcp = '''
# The loopback network interface
auto lo
iface lo inet loopback

# The primary network interface
allow-hotplug {ETH_IF}
iface {ETH_IF} inet dhcp
hostname {HOSTNAME}

# The wireless network interface
allow-hotplug {WLAN_IF}
iface {WLAN_IF} inet dhcp
hostname {HOSTNAME}
    wpa-conf /etc/wpa_supplicant/wpa_supplicant.conf
'''


config_file_static = '''
# The loopback network interface
auto lo
iface lo inet loopback

# The primary network interface
allow-hotplug {ETH_IF}
auto {ETH_IF}
iface {ETH_IF} inet static
address {ETH_IP}
netmask {ETH_MASK}
gateway {ETH_GW}
dns-nameservers {DNS}

# The wireless network interface
allow-hotplug {WLAN_IF}
iface {WLAN_IF} inet dhcp
hostname {HOSTNAME}
    wpa-conf /etc/wpa_supplicant/wpa_supplicant.conf
'''


dhcpcd_file_static = '''
# Inform the DHCP server of our hostname for DDNS.
hostname

# Use the hardware address of the interface for the Client ID.
clientid

# Persist interface configuration when dhcpcd exits.
persistent

# Rapid commit support.
option rapid_commit

# A list of options to request from the DHCP server.
option domain_name_servers, domain_name, domain_search, host_name
option classless_static_routes

# Most distributions have NTP support.
option ntp_servers

# A ServerID is required by RFC2131.
require dhcp_server_identifier

# Generate Stable Private IPv6 Addresses instead of hardware based ones
slaac private

# A hook script is provided to lookup the hostname if not set by the DHCP
# server, but it should not be run by default.
nohook lookup-hostname

interface {ETH_IF}
        static ip_address={ETH_IP_SLASH}
        static routers={ETH_GW}
        static domain_name_servers={DNS}
'''


dhcpcd_file_dhcp = '''
# Inform the DHCP server of our hostname for DDNS.
hostname

# Use the hardware address of the interface for the Client ID.
clientid

# Persist interface configuration when dhcpcd exits.
persistent

# Rapid commit support.
option rapid_commit

# A list of options to request from the DHCP server.
option domain_name_servers, domain_name, domain_search, host_name
option classless_static_routes

# Most distributions have NTP support.
option ntp_servers

# A ServerID is required by RFC2131.
require dhcp_server_identifier

# Generate Stable Private IPv6 Addresses instead of hardware based ones
slaac private

# A hook script is provided to lookup the hostname if not set by the DHCP
# server, but it should not be run by default.
nohook lookup-hostname
'''


def network_save(button):
    # fetch network settings
    setting_names = ['HOSTNAME', 'DNS', 'ETH_USE_DHCP', 'ETH_IP', 'ETH_MASK', 'ETH_GW']

    values = {}
    for name in setting_names:                  # fetch settings by name
        values[name] = setting_get_merged(name)

    # verify iP addresses
    if not values['ETH_USE_DHCP']:      # not using dhcp, check IP addresses
        ips = ['DNS', 'ETH_IP', 'ETH_MASK', 'ETH_GW']

        for name in ips:                # go through the names with IP addresses
            good = False
            try:
                IP(values[name])      # let IPy try to read the addr
                good = True
            except Exception as exc:
                app_log.warning(f"failed to convert {values[name]} to IP: {str(exc)}")

            if not good:
                dialog(shared.main_loop, shared.current_body, f"The IP address {values[name]} seems to be invalid!")
                return

    # if hostname seems to be empty
    if not values['HOSTNAME']:
        dialog(shared.main_loop, shared.current_body, f"Hostname seems to be invalid!")
        return

    # get interface names
    values['ETH_IF'] = get_eth_iface('eth0')
    values['WLAN_IF'] = get_wifi_iface('wlan0')

    # if not using dhcp, we also need ip in slash formwat for dhcpcd.conf
    if not values['ETH_USE_DHCP']:
        ip_slash_mask = '{}/{}'.format(values['ETH_IP'], values['ETH_MASK'])
        ip_slash_format = IP(ip_slash_mask, make_net=True).strNormal()
        values['ETH_IP_SLASH'] = ip_slash_format

    # --------------------------
    # for /etc/network/interface
    # select the right template and fill with values
    config = config_file_dhcp if values['ETH_USE_DHCP'] else config_file_static
    config = config.format(**values)

    # write network settings to file
    with open("/etc/network/interfaces", "wt") as text_file:
        text_file.write(config)

    # --------------------------
    # for /etc/dhcpcd.conf
    # select the right template and fill with values
    config = dhcpcd_file_dhcp if values['ETH_USE_DHCP'] else dhcpcd_file_static
    config = config.format(**values)

    # write network settings to file
    with open("/etc/dhcpcd.conf", "wt") as text_file:
        text_file.write(config)

    # --------------------------
    # for /etc/hostname
    # write new hostname to file
    with open("/etc/hostname", "wt") as text_file:
        text_file.write(values['HOSTNAME'])

    system('sync')
    dialog(shared.main_loop, shared.current_body,
           "Your network settings have been saved. Restart your device for the changes to take effect.")

    back_to_main_menu(None)
