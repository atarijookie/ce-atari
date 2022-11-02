import os
import shutil
import subprocess
from pythonping import ping
import logging
from shared import print_and_log, setting_get_bool, get_symlink_path_for_letter, umount_if_mounted, \
    options_to_string, LETTER_SHARED, text_to_file, text_from_file
import shared


def is_shared_mounted(shared_mount_path):
    """ see if shared drive is already mounted or not  """
    result = subprocess.run(['mount'], stdout=subprocess.PIPE)        # run 'mount' command
    result = result.stdout.decode('utf-8')  # get output as string
    lines = result.split('\n')              # split whole result to lines

    for line in lines:                      # go through lines
        parts = line.split(' ')             # split '/dev/sda1 on /mnt/drive type ...' to items

        if len(parts) < 3:                  # some line, which couldn't been split to at least 3 parts? skip it
            continue

        mount_dir = parts[2]                # get mount point

        if mount_dir == shared_mount_path:  # if this existing mount point is shared mount path, then shared is mounted
            return True

    # shared mount point wasn't found, shared drive not mounted
    return False


def mount_shared():
    """ this function checks for shared drive settings, mounts drive if needed """
    shared_enabled = setting_get_bool('SHARED_ENABLED')
    mount_path = "/mnt/shared"              # get where it should be mounted
    symlink_path = get_symlink_path_for_letter(LETTER_SHARED)       # where the shared drive will be symlinked

    if not shared_enabled:                  # if shared drive not enabled, don't do rest, possibly umount
        text_to_file('', shared.MOUNT_SHARED_CMD_LAST)     # clear the mount command
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

    cmd = cmd.strip()       # remove trailing and leading whitespaces

    # check if shared is mounted, if not mounted, do mount anyway, even if cmd is the same below
    is_mounted = is_shared_mounted(mount_path)
    print_and_log(logging.INFO, f"mount_shared: shared drive is_mounted: {is_mounted}")

    # if already mounted AND no change in the created command, nothing to do here
    # (but if not mounted, proceed with mounting, even if the command hasn't changed)
    mount_shared_cmd_last = text_from_file(shared.MOUNT_SHARED_CMD_LAST)

    if is_mounted and cmd == mount_shared_cmd_last:
        print_and_log(logging.INFO, f"mount_shared: mount command not changed, not mounting")
        return

    text_to_file(cmd, shared.MOUNT_SHARED_CMD_LAST)     # store this cmd

    cmd += f" > {shared.MOUNT_LOG_FILE} 2>&1 "     # append writing of stdout and stderr to file

    # command changed, we should execute it
    if os.path.exists(symlink_path):        # remove symlink if it exists
        os.unlink(symlink_path)

    os.makedirs(mount_path, exist_ok=True)  # create dir if not exist
    umount_if_mounted(mount_path)           # possibly umount

    good = False
    try:
        print_and_log(logging.INFO, f'mount_shared: cmd: {cmd}')
        status = os.system(cmd)         # try mounting
        good = status == 0              # good if status is 0
        print_and_log(logging.INFO, f'mount_shared: good={good}')

        os.symlink(mount_path, symlink_path)        # symlink from mount path to symlink path
    except Exception as exc:
        print_and_log(logging.INFO, f'mount_shared: mount failed : {str(exc)}')

    if not good:        # mount failed, copy mount log
        try:
            shutil.copy(shared.MOUNT_LOG_FILE, mount_path)
        except Exception as exc:
            print_and_log(logging.INFO, f'mount_shared: copy of log file failed : {str(exc)}')
