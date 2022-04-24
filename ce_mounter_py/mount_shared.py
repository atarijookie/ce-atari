import os
import shutil
from pythonping import ping
import logging
from shared import print_and_log, setting_get_bool, get_mount_path_for_letter, umount_if_mounted, \
    options_to_string, LETTER_SHARED
import shared

mount_shared_cmd_last = ''


def mount_shared():
    """ this function checks for shared drive settings, mounts drive if needed """
    global mount_shared_cmd_last

    shared_enabled = setting_get_bool('SHARED_ENABLED')
    mount_path = get_mount_path_for_letter(LETTER_SHARED)       # get where it should be mounted

    if not shared_enabled:                  # if shared drive not enabled, don't do rest, possibly umount
        mount_shared_cmd_last = ''          # clear the mount command
        print_and_log(logging.DEBUG, f"mount_shared: SHARED_ENABLED={shared_enabled}, not mounting")

        if os.path.exists(mount_path):      # got the mount path? do the umount
            umount_if_mounted(mount_path, delete=True)   # possibly umount

        return

    addr = shared.settings.get('SHARED_ADDRESS')
    path_ = shared.settings.get('SHARED_PATH')
    user = shared.settings.get('SHARED_USERNAME')
    pswd = shared.settings.get('SHARED_PASSWORD')

    if not addr or not path_:   # address or path not provided? don't do the rest
        print_and_log(logging.DEBUG, f"mount_shared: addr={addr}, path={path_}. not mounting")
        return

    # ping addr to see if it's alive, don't mount if not alive
    resp = ping(addr, timeout=1, count=1)       # ping only once

    if resp.packets_lost > 0:                   # if the only packet was lost, don't mount
        print_and_log(logging.INFO, f"mount_shared: ping didn't get response from {addr}, not mounting")
        return

    nfs_not_samba = setting_get_bool('SHARED_NFS_NOT_SAMBA')

    if nfs_not_samba:       # NFS shared drive
        options = options_to_string({'username': user, 'password': pswd, 'vers': 3})
        cmd = f'mount -t nfs {options} {addr}:/{path_} {mount_path}'
    else:                   # cifs / samba / windows share
        options = options_to_string({'username': user, 'password': pswd})
        cmd = f'mount -t cifs {options} //{addr}/{path_} {mount_path}'

    # TODO: check if shared is mounted, if not mounted, do mount anyway, even if cmd is the same below

    # if no change in the created command, nothing to do here
    if cmd == mount_shared_cmd_last:
        print_and_log(logging.INFO, f"mount_shared: mount command not changed, not mounting")
        return

    mount_shared_cmd_last = cmd             # store this cmd

    cmd += " > /tmp/ce/mount.log 2>&1 "     # append writing of stdout and stderr to file

    # command changed, we should execute it
    os.makedirs(mount_path, exist_ok=True)  # create dir if not exist
    umount_if_mounted(mount_path)           # possibly umount

    good = False
    try:
        print_and_log(logging.INFO, f'mount_shared: cmd: {cmd}')
        status = os.system(cmd)         # try mounting
        good = status == 0              # good if status is 0
        print_and_log(logging.INFO, f'mount_shared: good={good}')
    except Exception as exc:
        print_and_log(logging.INFO, f'mount_shared: mount failed : {str(exc)}')

    if not good:        # mount failed, copy mount log
        try:
            shutil.copy('/tmp/ce/mount.log', mount_path)
        except Exception as exc:
            print_and_log(logging.INFO, f'mount_shared: copy of log file failed : {str(exc)}')
