import os
import shutil
from pythonping import ping
import logging
from wrapt_timeout_decorator import timeout
from shared import print_and_log, setting_get_bool, get_symlink_path_for_letter, umount_if_mounted, \
    text_to_file, text_from_file, letter_shared, unlink_without_fail, FILE_MOUNT_CMD_SAMBA, FILE_MOUNT_CMD_NFS, \
    MOUNT_SHARED_CMD_LAST, load_one_setting, MOUNT_DIR_SHARED, is_shared_mounted, symlink_if_needed


def get_shared_mount_command(nfs_not_samba):
    """ Function will get the formatted mount command based on share type it will be used for.
        It will create or just read the mount command from /ce/settings/mount_cmd_...txt file,
        where user has the option to modify this command if his shared folder requires additional / different
        parameters than the default ones.
    """

    # get the right mount command file for this share type
    mount_cmd_file = FILE_MOUNT_CMD_NFS if nfs_not_samba else FILE_MOUNT_CMD_SAMBA

    print_and_log(logging.DEBUG, f"get_shared_mount_command: mount_cmd_file is: {mount_cmd_file}")

    if not os.path.exists(mount_cmd_file):      # if the file doesn't exist, write default command in that file
        if nfs_not_samba:   # NFS shared drive
            default_cmd = ('mount -t nfs -o username={username},password={password},vers=3 '
                           '{server_address}:/{path_on_server} {mount_path}')
        else:               # cifs / samba / windows share
            default_cmd = ('mount -t cifs -o username={username},password={password} '
                           '//{server_address}/{path_on_server} {mount_path}')

        print_and_log(logging.DEBUG, f"get_shared_mount_command: mount_cmd_file {mount_cmd_file} does not exist")
        print_and_log(logging.DEBUG, f"get_shared_mount_command: will write this cmd in that file: {default_cmd}")

        # write the default mount command to file
        text_to_file(default_cmd, mount_cmd_file)

    # get the mount command from file - either the default value, or user customized value
    mount_cmd = text_from_file(mount_cmd_file)
    print_and_log(logging.DEBUG, f"get_shared_mount_command: will use this mount_cmd: {mount_cmd}")
    return mount_cmd


@timeout(10)
def mount_shared():
    """ this function checks for shared drive settings, mounts drive if needed """
    shared_enabled = setting_get_bool('SHARED_ENABLED')
    mount_path = MOUNT_DIR_SHARED              # get where it should be mounted
    symlink_path = get_symlink_path_for_letter(letter_shared())       # where the shared drive will be symlinked

    if not shared_enabled:                  # if shared drive not enabled, don't do rest, possibly umount
        text_to_file('', MOUNT_SHARED_CMD_LAST)     # clear the mount command
        print_and_log(logging.DEBUG, f"mount_shared: SHARED_ENABLED={shared_enabled}, not mounting")

        if os.path.exists(mount_path):      # got the mount path? do the umount
            umount_if_mounted(mount_path, delete=True)   # possibly umount

        return

    addr = load_one_setting('SHARED_ADDRESS')
    path_ = load_one_setting('SHARED_PATH')
    user = load_one_setting('SHARED_USERNAME')
    pswd = load_one_setting('SHARED_PASSWORD')

    if not addr or not path_:   # address or path not provided? don't do the rest
        print_and_log(logging.DEBUG, f"mount_shared: addr={addr}, path={path_}. not mounting")
        return

    # ping addr to see if it's alive, don't mount if not alive
    resp = ping(addr, timeout=1, count=1)       # ping only once

    if resp.packets_lost > 0:                   # if the only packet was lost, don't mount
        print_and_log(logging.INFO, f"mount_shared: ping didn't get response from {addr}, not mounting")
        return

    nfs_not_samba = setting_get_bool('SHARED_NFS_NOT_SAMBA')
    cmd = get_shared_mount_command(nfs_not_samba)       # get the default / customized mount command and format it
    cmd = cmd.format(username=user, password=pswd, server_address=addr, path_on_server=path_, mount_path=mount_path)
    cmd = cmd.strip()       # remove trailing and leading whitespaces

    # check if shared is mounted, if not mounted, do mount anyway, even if cmd is the same below
    is_mounted = is_shared_mounted()
    print_and_log(logging.INFO, f"mount_shared: shared drive is_mounted: {is_mounted}")

    # if already mounted AND no change in the created command, nothing to do here
    # (but if not mounted, proceed with mounting, even if the command hasn't changed)
    mount_shared_cmd_last = text_from_file(MOUNT_SHARED_CMD_LAST)

    if is_mounted and cmd == mount_shared_cmd_last:     # if is mounted and command not changed, don't remount
        print_and_log(logging.INFO, f"mount_shared: mount command not changed, not mounting")
        symlink_if_needed(mount_path, symlink_path)     # create symlink, but only if needed
        return

    text_to_file(cmd, MOUNT_SHARED_CMD_LAST)     # store this cmd

    mount_log_file = os.path.join(os.getenv('LOG_DIR'), 'mount.log')
    cmd += f" > {mount_log_file} 2>&1 "     # append writing of stdout and stderr to file

    # command changed, we should execute it
    if os.path.exists(symlink_path):        # remove symlink if it exists
        unlink_without_fail(symlink_path)

    os.makedirs(mount_path, exist_ok=True)  # create dir if not exist
    umount_if_mounted(mount_path)           # possibly umount

    good = False
    try:
        print_and_log(logging.INFO, f'mount_shared: cmd: {cmd}')
        status = os.system(cmd)         # try mounting
        good = status == 0              # good if status is 0
        print_and_log(logging.INFO, f'mount_shared: good={good}')

        symlink_if_needed(mount_path, symlink_path)     # create symlink, but only if needed
    except Exception as exc:
        print_and_log(logging.INFO, f'mount_shared: mount failed : {str(exc)}')

    if not good:        # mount failed, copy mount log
        try:
            shutil.copy(mount_log_file, mount_path)
        except Exception as exc:
            print_and_log(logging.INFO, f'mount_shared: copy of log file failed : {str(exc)}')
