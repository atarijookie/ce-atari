import urwid
from urwid_helpers import create_my_button, create_header_footer, create_edit, MyCheckBox, dialog
from utils import settings_load, settings_save, on_cancel, back_to_main_menu, setting_get_bool, on_editline_changed, \
    on_checkbox_changed
import shared
import netifaces as ni
import dns.resolver
import subprocess


def create_setting_row(label, what, value, col1w, col2w, reverse=False, setting_name=None):
    if what == 'checkbox':      # for checkbox
        checked = setting_get_bool(setting_name)
        widget = MyCheckBox('', state=checked, on_state_change=on_net_checkbox_changed)
        widget.setting_name = setting_name
        label = "   " + label
    elif what == 'edit':        # for edit line
        widget, _ = create_edit(setting_name, col2w, on_editline_changed)
        label = "   " + label
    elif what == 'text':
        widget = urwid.Text(value)

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

    return cols


def get_eth_iface():
    return get_iface_for('eth0', 'enp')


def get_wifi_iface():
    return get_iface_for('wlan0', 'wlp')


def get_iface_for(old_name, predictable_name):
    # go through interfaces, find ethernet
    ifaces = ni.interfaces()

    eth = None

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


def network_save(button):
    pass


