from os import system
from time import time
import urwid
from loguru import logger as app_log
import copy
import threading
from urwid_helpers import create_my_button, create_header_footer, create_edit, MyCheckBox, dialog
from utils import settings_load, on_cancel, back_to_main_menu, setting_get_bool, on_editline_changed, \
    on_checkbox_changed, setting_get_merged, text_to_file, system_custom
import shared
from IPy import IP


def ip_prefix_to_netmask(prefix):
    """ convert IP prefix number to netmask, e.g. 24 to 255.255.255.0 """
    prefix = int(prefix)        # from str to int if needed

    mask_binary = ""

    for i in range(32):         # generate binary string, e.g. 11111111111110000
        mask_binary += '1' if i < prefix else '0'

        if i in [7, 15, 23]:    # separate bytes with columns
            mask_binary += ':'

    parts = mask_binary.split(':')  # split to individual bytes

    for i in range(4):              # from binary to decimal
        parts[i] = str(int(parts[i], 2))

    mask = ".".join(parts)          # individual decimal numbers to net.mask.with.dots
    return mask


def netmask_to_ip_prefix(netmask):
    """ convert netmask to IP prefix number, e.g. 255.255.255.0 to 24 """

    if not netmask:         # None or empty netmask?
        return 0

    ones = 0
    parts = netmask.split('.')                  # split 255.255.255.0 to ["255", "255", "255", "0"]

    for part in parts:
        int10 = int(part)                       # string to int
        int2 = bin(int10)                       # decimal to binary, with '0b' in front
        int2_str = int2.replace("0", "")        # remove zeros
        int2_str = int2_str.replace("b", "")    # remove 'b'
        ones += len(int2_str)                   # add length of the rest (just ones) to total count of ones

    return ones


def get_interface():
    """ fetch interface names for ethernet and wifi using nmcli """
    # run 'nmcli -t -f DEVICE,TYPE,CON-UUID device'
    # result = subprocess.run(['nmcli', '-t', '-f', 'DEVICE,TYPE,CON-UUID', 'device'], stdout=subprocess.PIPE)
    # result = result.stdout.decode('utf-8')  # get output as string
    result, _ = system_custom('nmcli -t -f DEVICE,TYPE,CON-UUID device')
    lines = result.split('\n')              # split whole result to lines

    eth = wifi = None

    # output from 'nmcli -t device' looks like this:
    # eno1:ethernet:connected:Wired connection 1
    for line in lines:                      # go through the individual lines
        chunks = line.split(':')

        if not chunks or len(chunks) != 3:  # ignore weird lines
            continue

        device, iftype, con_uuid = chunks

        if iftype == 'ethernet' and not eth:        # found ethernet and don't have ethernet yet? store it
            eth = (device, con_uuid)

        if iftype == 'wifi' and not wifi:           # found wifi and don't have wifi yet? store it
            wifi = (device, con_uuid)

    app_log.debug(f"found networks: eth: {eth}, wifi: {wifi}")
    return eth, wifi


def get_interface_settings(con_uuid):
    """ fetch interface settings specified device using nmcli """
    # nmcli -t con show [connection_uuid]
    # result = subprocess.run(['nmcli', '-t', 'con', 'show', con_uuid], stdout=subprocess.PIPE)
    # result = result.stdout.decode('utf-8')  # get output as string
    result, _ = system_custom(f'nmcli -t --show-secrets con show {con_uuid}')
    lines = result.split('\n')              # split whole result to lines

    # output from 'nmcli -t con show con_uuid' looks like this:
    # GENERAL.DEVICE:eno1
    # GENERAL.TYPE:ethernet
    # GENERAL.HWADDR:40:A8:F0:A5:77:AF
    # GENERAL.MTU:1500
    # GENERAL.STATE:100 (connected)
    # GENERAL.CONNECTION:Wired connection 1
    # GENERAL.CON-PATH:/org/freedesktop/NetworkManager/ActiveConnection/1
    # WIRED-PROPERTIES.CARRIER:on
    # IP4.ADDRESS[1]:192.168.123.55/24
    # IP4.GATEWAY:192.168.123.1
    # IP4.ROUTE[1]:dst = 0.0.0.0/0, nh = 192.168.123.1, mt = 100
    # IP4.ROUTE[2]:dst = 192.168.123.0/24, nh = 0.0.0.0, mt = 100
    # IP4.DNS[1]:192.168.123.1
    # 802-11-wireless.ssid:jooknet2
    # 802-11-wireless-security.psk:yourpasswordhere

    settings = {'use_dhcp': True, 'ip': None, 'mask': None, 'gateway': None, 'dns': None,
                'WIFI_SSID': None, 'WIFI_PSK': None}

    for line in lines:                          # go through the individual lines
        chunks = line.split(':', maxsplit=1)

        if not chunks or len(chunks) != 2:      # ignore weird lines
            continue

        key, value = chunks                     # split list to individual vars
        nmcli_to_simple = {'GENERAL.DEVICE': 'device', 'IP4.ADDRESS[1]': 'ip', 'IP4.GATEWAY': 'gateway',
                           'IP4.DNS[1]': 'dns', 'ipv4.method': 'use_dhcp',
                           '802-11-wireless.ssid': 'WIFI_SSID', '802-11-wireless-security.psk': 'WIFI_PSK'}

        if key not in nmcli_to_simple.keys():   # this key is not what we're looking for, skip it
            continue

        key = nmcli_to_simple.get(key)          # translate nmcli key to simple key

        if key == 'use_dhcp':                   # if this is use_dhcp setting, then it's dhcp when value is 'auto'
            value = (value == 'auto')

        if key == 'ip':                         # we're dealing with IP + mask here?
            ip, mask = value, None              # start with value + no mask

            if '/' in ip:                                   # if '/' was found in ip string
                ip, prefix = ip.split('/', maxsplit=1)      # split string to ip and prefix
                mask = ip_prefix_to_netmask(prefix)         # prefix to netmask

            settings['ip'] = ip
            settings['mask'] = mask
        else:                                   # not dealing with Ip here, just store key + value
            settings[key] = value

    app_log.debug(f"device: {con_uuid}, settings: {settings}")
    return settings


def cons_get_or_create(eth_not_wifi, values):
    """ get connections, and if no connections are available, create one """

    cons = get_cons_for_if(eth_not_wifi)        # get existing connections

    if cons:                # got connections? return them
        return cons

    values['if_type'] = 'ethernet' if eth_not_wifi else 'wifi'
    values['con_name'] = "con-" + values['if_type']          # con-eth or con-wifi will be used as connection name

    # add single connection
    cmd = 'nmcli con add con-name {con_name} ifname {if_name} type {if_type}'.format(**values)

    if not eth_not_wifi:        # for wifi also add ssid
        cmd += ' ssid {WIFI_SSID}'.format(**values)

    system_custom(cmd)          # execute command

    cons = get_cons_for_if(eth_not_wifi)  # get all existing connections
    return cons


def get_cons_for_if(eth_not_wifi):
    """ fetch all connections for ethernet or wifi """
    # nmcli -t con show
    # result = subprocess.run(['nmcli', '-t', 'con', 'show'], stdout=subprocess.PIPE)
    # result = result.stdout.decode('utf-8')  # get output as string
    result, _ = system_custom('nmcli -t con show')
    lines = result.split('\n')              # split whole result to lines

    # output from 'nmcli -t con show' looks like this:
    # Wired connection 1:275b0764-94eb-4626-ba69-26d861af541c:802-3-ethernet:eno1
    # docker0:8b0ddfde-1bac-4333-87d4-967e1f4b3814:bridge:docker0

    wanted_type = 'ethernet' if eth_not_wifi else 'wireless'
    uuids = []

    for line in lines:                          # go through the individual lines
        chunks = line.split(':')

        if not chunks or len(chunks) < 3:       # ignore weird lines
            continue

        if wanted_type not in chunks[2]:        # this is not the wanted device type (e.g. ethernet / wifi)
            continue

        uuids.append(chunks[1])                 # append uuid to list

    app_log.debug(f"wanted_type: {wanted_type}, uuids: {uuids}")
    return uuids


def create_setting_row(label, what, value, col1w, col2w, reverse=False, setting_name=None, return_widget=False,
                       is_pass=False):
    wret = None     # widget to return

    if what == 'checkbox':      # for checkbox
        checked = setting_get_bool(setting_name)
        widget = MyCheckBox('', state=checked, on_state_change=on_net_checkbox_changed)
        wret = widget
        widget.setting_name = setting_name
        label = "   " + label
    elif what == 'edit':        # for edit line
        mask = '*' if is_pass else None
        widget, _ = create_edit(setting_name, col2w, on_editline_changed, mask=mask)
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


def load_network_settings():
    # get hostname
    with open('/etc/hostname') as f:
        hostname = f.read()
        hostname = hostname.strip()
        shared.settings['HOSTNAME'] = hostname

    shared.settings['ETH_PRESENT'] = False
    shared.settings['WIFI_PRESENT'] = False

    # get network interfaces
    eth, wifi = get_interface()

    if eth:     # got eth device name?
        shared.settings['ETH_PRESENT'] = True

        setts = get_interface_settings(eth[1])      # get settings
        shared.settings['ETH_IP'] = setts['ip']
        shared.settings['ETH_MASK'] = setts['mask']
        shared.settings['ETH_GW'] = setts['gateway']
        shared.settings['ETH_USE_DHCP'] = setts['use_dhcp']
        shared.settings['DNS'] = setts['dns']

    if wifi:    # got wifi device name?
        shared.settings['WIFI_PRESENT'] = True

        setts = get_interface_settings(wifi[1])     # get settings
        shared.settings['WIFI_IP'] = setts['ip']
        shared.settings['WIFI_MASK'] = setts['mask']
        shared.settings['WIFI_GW'] = setts['gateway']
        shared.settings['WIFI_USE_DHCP'] = setts['use_dhcp']
        shared.settings['DNS'] = setts['dns']
        shared.settings['WIFI_SSID'] = setts['WIFI_SSID']
        shared.settings['WIFI_PSK'] = setts['WIFI_PSK']


def network_create(button):
    # if saving thread still running, don't show network screen
    if shared.thread_save_running:
        dialog(shared.main_loop, shared.current_body,
               "Your last network changes are still being applied. Please try again in a moment.")
        return

    _, res = system_custom('which nmcli')         # figure out if we got nmcli installed
    got_nmcli = res == 0

    settings_load()

    wifi = None
    if got_nmcli:                       # load network settings only if got nmcli installed
        load_network_settings()
        _, wifi = get_interface()

    header, footer = create_header_footer('Network settings')

    body = []
    body.append(urwid.Divider())

    col1w = 16
    col2w = 18

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

    if wifi:        # show wifi settings only if got wifi
        # wifi settings
        cols = create_setting_row('Wifi', 'title', '', col1w, col2w)
        body.append(cols)

        cols = create_setting_row('Use DHCP', 'checkbox', False, col1w, col2w, False, 'WIFI_USE_DHCP')
        body.append(cols)

        cols = create_setting_row('IP address', 'edit', '', col1w, col2w, False, 'WIFI_IP')
        body.append(cols)

        cols = create_setting_row('Mask', 'edit', '', col1w, col2w, False, 'WIFI_MASK')
        body.append(cols)

        cols = create_setting_row('Gateway', 'edit', '', col1w, col2w, False, 'WIFI_GW')
        body.append(cols)
        body.append(urwid.Divider())

        # wifi connection
        cols = create_setting_row('Wifi connection', 'title', '', col1w, col2w)
        body.append(cols)

        cols = create_setting_row('Network', 'edit', '', col1w, col2w, False, 'WIFI_SSID')
        body.append(cols)

        cols = create_setting_row('Password', 'edit', '', col1w, col2w, False, 'WIFI_PSK', is_pass=True)
        body.append(cols)
        body.append(urwid.Divider())

    # add save + cancel button
    button_save = create_my_button(" Save", network_save)
    button_cancel = create_my_button("Cancel", on_cancel)
    buttons = urwid.GridFlow([button_save, button_cancel], 10, 1, 1, 'center')
    body.append(buttons)

    w_body = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), 'center', 36)
    shared.main.original_widget = urwid.Frame(w_body, header=header, footer=footer)

    if not got_nmcli:       # without nmcli show warning
        dialog(shared.main_loop, shared.current_body,
               "nmcli tool not installed. This setting screen doesn't work without it.")


def on_net_checkbox_changed(widget, state):
    on_checkbox_changed(widget.setting_name, state)


def network_settings_changed(eth_not_wifi, values):
    """ check if settings for eth or wifi have changed or not """
    iface = 'eth' if eth_not_wifi else 'wifi'
    iface_present = values['ETH_PRESENT'] if eth_not_wifi else values['WIFI_PRESENT']

    if not iface_present:   # interface is not present, we're ignoring any changes and not saving them
        app_log.debug(f'{iface} - iface_present: {iface_present}, so network_settings_changed answers False')
        return False

    if_keys = ['DNS', 'ETH_USE_DHCP', 'ETH_IP', 'ETH_MASK', 'ETH_GW'] if eth_not_wifi else \
        ['DNS', 'WIFI_USE_DHCP', 'WIFI_IP', 'WIFI_MASK', 'WIFI_GW', 'WIFI_SSID', 'WIFI_PSK']

    for key in if_keys:                             # go through watched keys
        if key in shared.settings_changed.keys():   # changed key was found
            app_log.debug(f'{iface} - changed key {key} found, so network_settings_changed answers True')
            return True                             # settings have changed

    app_log.debug(f'{iface} - no changed key found, so network_settings_changed answers False')
    return False        # no settings changed here


def network_settings_good(eth_not_wifi, values):
    """ check if IP addresses for this interface are good or not """
    using_dhcp = values['ETH_USE_DHCP'] if eth_not_wifi else values['WIFI_USE_DHCP']
    iface_present = values['ETH_PRESENT'] if eth_not_wifi else values['WIFI_PRESENT']
    changed = network_settings_changed(eth_not_wifi, values)    # something changed here or not?

    iface = 'eth' if eth_not_wifi else 'wifi'
    app_log.debug(f'{iface} - changed: {changed}, present: {iface_present}, using_dhcp: {using_dhcp}')

    # if interface not present or using dhcp, don't check IP addresses and pretend IPs are all good
    if not changed or not iface_present or using_dhcp:
        app_log.debug(f'{iface} - network_settings_good returning True')
        return True

    ip_keys = ['DNS', 'ETH_IP', 'ETH_MASK', 'ETH_GW'] if eth_not_wifi else ['DNS', 'WIFI_IP', 'WIFI_MASK', 'WIFI_GW']

    for name in ip_keys:        # go through the names with IP addresses
        good = False

        try:
            IP(values[name])    # let IPy try to read the addr
            good = True
        except Exception as exc:
            app_log.warning(f"failed to convert {values[name]} to IP: {str(exc)}")

        if not good:
            dialog(shared.main_loop, shared.current_body, f"The IP address {values[name]} seems to be invalid!")
            app_log.debug(f'{iface} - network_settings_good returning False')
            return False

    # if got here, no exception occured, we're good
    app_log.debug(f'{iface} - network_settings_good returning True (at the end)')
    return True


def save_net_settings_dhcp(eth_not_wifi, values):
    """ save network settings - with dhcp enabled """
    cons = cons_get_or_create(eth_not_wifi, values)         # get all existing connections

    if not cons:                # no connection at this point?
        app_log.warning(f'cons empty, saving will not work!')
        return

    values['uuid'] = cons[0]  # get uuid of 0th connection
    system_custom('nmcli con mod {uuid} ipv4.method auto'.format(**values))     # set to dhcp before removing GW & IP

    # we need to remove gw and ip when enabling dhcp, otherwise this device will have old static + new dhcp address
    system_custom('nmcli con mod {uuid} ipv4.gateway ""'.format(**values), shell=True)  # remove gw before removing ip
    system_custom('nmcli con mod {uuid} ipv4.addresses ""'.format(**values), shell=True)    # remove ip

    system_custom('nmcli con up {uuid}'.format(**values))       # connection up


def save_net_settings_static(eth_not_wifi, values):
    """ save network settings - with static ip address """
    cons = cons_get_or_create(eth_not_wifi, values)         # get all existing connections

    if not cons:                # no connection at this point?
        app_log.warning(f'cons empty, saving will not work!')
        return

    for uuid in cons:           # existing connections down
        system_custom(f'nmcli con down {uuid}')

    values['prefix'] = netmask_to_ip_prefix(values['mask'])  # mask from '255.255.255.0' to 24
    app_log.debug(f'{values["iface_type"]} - using values_out: {values}')

    values['uuid'] = cons[0]    # get uuid of 0th connection
    system_custom('nmcli con mod {uuid} ipv4.addresses {ip4}/{prefix}'.format(**values))
    system_custom('nmcli con mod {uuid} ipv4.gateway {gw4}'.format(**values))
    system_custom('nmcli con mod {uuid} ipv4.dns {dns}'.format(**values))
    system_custom('nmcli con mod {uuid} ipv4.method manual'.format(**values))       # set 'manual' after ip has been set
    system_custom(f'nmcli con up {uuid}'.format(**values))                          # connection up


def save_net_settings(eth_not_wifi, values_in):
    """ save network settings (ip, mask, gateway or use DHCP) for ethernet of wifi """
    iface_type = 'eth' if eth_not_wifi else 'wifi'
    using_dhcp = values_in['ETH_USE_DHCP'] if eth_not_wifi else values_in['WIFI_USE_DHCP']
    setting_names = ['DNS', 'ETH_IP', 'ETH_MASK', 'ETH_GW'] if eth_not_wifi else ['DNS', 'WIFI_IP', 'WIFI_MASK', 'WIFI_GW']
    format_names = ['dns', 'ip4', 'mask', 'gw4']

    values_out = copy.deepcopy(values_in)           # copy all the original values in
    for i, out_name in enumerate(format_names):     # for these output setting names
        name_in = setting_names[i]                  # fetch input name
        values_out[out_name] = values_in[name_in]   # for output key-value fetch input value

    # get interface name
    eth, wifi = get_interface()
    values_out['iface_type'] = iface_type
    values_out['if_name'] = eth[0] if eth_not_wifi else wifi[0]

    if using_dhcp:              # for dhcp - enable auto method
        save_net_settings_dhcp(eth_not_wifi, values_out)
    else:                       # for static ip - add new connection
        save_net_settings_static(eth_not_wifi, values_out)


def wifi_get_active_ssid():
    result, status = system_custom('nmcli -t -f ACTIVE,SSID dev wifi')

    # example output of above command...
    # yes:jooknet2
    # no:jooknet2
    # no:
    # no:W-HMR-17
    # no:TP-Link_BC2B

    resp = {'WIFI_SSID': None, 'WIFI_PSK': None}

    lines = result.split('\n')      # whole output to lines

    for line in lines:              # find line with active connection
        parts = line.split(':')

        if not parts or len(parts) != 2:    # skip weird lines
            continue

        if parts[0] == 'yes':       # found active connection?
            return parts[1]

    return None


def save_wifi_settings(values):
    """ save wifi SSID and PSK """
    _, wifi = get_interface()
    values_out = {'device': wifi[0], 'ssid': values['WIFI_SSID'], 'psk': values['WIFI_PSK']}
    app_log.debug(f'save_wifi_settings - values: {values_out}')

    cons = get_cons_for_if(False)       # get existing connections for wifi

    for uuid in cons:                   # delete existing wifi connections
        app_log.debug(f'deleting wifi connection {uuid}')
        system_custom(f'nmcli con delete {uuid}')

    system_custom('nmcli radio wifi on')
    system_custom('nmcli dev wifi connect {ssid} password "{psk}"'.format(**values_out), shell=True)
    system_custom('nmcli con up {ssid}'.format(**values_out))


def network_save_in_thread(values):
    """ Network settings here are applied using nmcli, with wifi connecting it can take 15 seconds,
        so we better run this in thread, so the app doesn't look stuck on saving.
    """

    start = time()
    app_log.debug('network_save_in_thread started')
    shared.thread_save_running = True

    if values['eth_changed']:   # if eth changed, save it
        app_log.debug(f'network_save_in_thread - applying eth changes')
        save_net_settings(True, values)

    if values['wifi_changed']:  # if wifi changed, save it
        app_log.debug(f'network_save_in_thread - applying wifi changes')
        save_wifi_settings(values)
        save_net_settings(False, values)

    # if something changed, sync and suggest restart
    if values['hostname_changed'] or values['eth_changed'] or values['wifi_changed']:
        app_log.debug(f'network_save_in_thread - sync caches and drives')
        system_custom('sync')

    # calc duration, log message, finish
    duration = time() - start
    app_log.debug(f'network_save_in_thread finished in {duration} seconds.')
    shared.thread_save_running = False


def network_save(button):
    """ Function will check if network settings seem to be correct and shows a warning if they aren't.
        Proceeds with saving if everything looks ok.
    """

    _, res = system_custom('which nmcli')         # figure out if we got nmcli installed
    got_nmcli = res == 0

    if not got_nmcli:                   # no nmcli? just quit now
        back_to_main_menu(None)
        return

    # fetch network settings
    setting_names = ['HOSTNAME', 'DNS',
                     'ETH_PRESENT', 'ETH_USE_DHCP', 'ETH_IP', 'ETH_MASK', 'ETH_GW',
                     'WIFI_PRESENT', 'WIFI_USE_DHCP', 'WIFI_IP', 'WIFI_MASK', 'WIFI_GW', 'WIFI_SSID', 'WIFI_PSK']

    # fetch settings by name
    values = {}
    for name in setting_names:
        values[name] = setting_get_merged(name)

    app_log.debug(f"values: {values}")

    # verify iP addresses for ethernet
    if not network_settings_good(True, values):
        return

    # verify iP addresses for wifi
    if not network_settings_good(False, values):
        return

    # check if hostname changed or not
    hostname_changed = 'HOSTNAME' in shared.settings_changed.keys()

    # if hostname seems to be empty
    if hostname_changed:
        if not values['HOSTNAME']:
            dialog(shared.main_loop, shared.current_body, f"Hostname seems to be invalid!")
            return

        # for /etc/hostname - write new hostname to file
        text_to_file(values['HOSTNAME'], "/etc/hostname")

    # eth and wifi settings changed or not?
    eth_changed = network_settings_changed(True, values)
    wifi_changed = network_settings_changed(False, values)

    # if nothing really changed, go to main menu
    if not eth_changed and not wifi_changed:
        back_to_main_menu(None)
        return

    # if saving thread still running, don't make it run twice
    if shared.thread_save_running:
        dialog(shared.main_loop, shared.current_body,
               "Your last network changes are still being applied. Please try again in a moment.")
        return

    values['eth_changed'] = eth_changed
    values['wifi_changed'] = wifi_changed
    values['hostname_changed'] = hostname_changed

    # create thread and run the eth and wifi saving in thread
    # note: ',' in (values,) is intentional, otherwise won't pass values as dict but as individual args without it.
    shared.thread_save = threading.Thread(target=network_save_in_thread, args=(values,))
    shared.thread_save.start()

    # back to main menu if managed to get here
    back_to_main_menu(None)

    # this dialog will show over main menu
    dialog(shared.main_loop, shared.current_body,
        "Your network settings are being applied now. It can take several seconds until they take effect.")

