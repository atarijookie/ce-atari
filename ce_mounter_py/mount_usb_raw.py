import os
from loguru import logger as app_log
from shared import setting_get_int, unlink_without_fail, DEV_TYPE_RAW, \
    symlink_if_needed, get_root_fs_device, trigger_reload_raw


def get_symlink_path_for_id(id_):
    path = os.path.join(os.getenv('MOUNT_DIR_RAW'), str(id_))  # construct path where the id_ should be mounted
    return path


def get_free_ids():
    """ Check which device IDs have RAW type (2) selected, check which of those are already used by some device
    and return only those which are free and can be used. """
    ids = []

    for id_ in range(8):
        key = f'ACSI_DEVTYPE_{id_}'

        # if id_ is not RAW, skip it
        if setting_get_int(key) != DEV_TYPE_RAW:
            continue

        # check if position at id_ is used or not
        path = get_symlink_path_for_id(id_)

        if not os.path.exists(path):        # file/link doesn't exist or it's broken? it's free
            ids.append(id_)

            unlink_without_fail(path)       # try to delete it if it exists (e.g. if broken)

    return ids


def is_raw_device_linked(device):
    """ check if this device is already symlinked or not """
    for id_ in range(8):
        path = get_symlink_path_for_id(id_)

        if not os.path.exists(path) or not os.path.islink(path):    # path doesn't exist or not a link, skip rest
            continue

        source = os.readlink(path)  # get link source

        if device == source:        # this device is source of this link, we got this device
            return True

    return False


def link_raw_device(device, acsi_id):
    path = get_symlink_path_for_id(acsi_id)  # create mount path for this id

    # create symlink from device to ACSI slot
    app_log.debug(f"mount_device as RAW: {device} -> {path}")
    return symlink_if_needed(device, path)  # create symlink, but only if needed


def find_and_mount_raw(root_devs):
    """ symlink all the RAW devices to configured ACSI slots """
    dev_root_fs = get_root_fs_device()

    if dev_root_fs in root_devs:        # if the root fs device is reported in root devs, just remove it
        root_devs.remove(dev_root_fs)
        app_log.debug(f"find_and_mount_raw: ignoring {dev_root_fs} which is mounted as '/' on linux")

    root_devs = [dev for dev in root_devs if not is_raw_device_linked(dev)]  # keep only not symlinked devices

    if not root_devs:           # no devices? quit
        app_log.debug(f"find_and_mount_raw: no devices to be linked were found, skipping")
        return

    # find which ACSI slots are configured as RAW and are free
    ids = get_free_ids()

    if not ids:
        app_log.warning(f"find_and_mount_raw: No free RAW IDs found, cannot mount RAW devices")
        return False

    # get lengths of found devices and available IDs
    len_devs = len(root_devs)
    len_ids = len(ids)

    if len_ids < len_devs:
        app_log.warning((f"find_and_mount_raw: not enough free IDs for RAW devices, won't be able "
                                        f"to mount {len_devs - len_ids} devices"))

    count = min(len_devs, len_ids)  # if we don't have enough free ids, mount just that lower count

    got_something = False           # each time device is attempted to be symlinked, this will be ORed with result

    for i in range(count):          # mount all the found partition devices, if possible
        device = root_devs[i]
        acsi_id = ids[i]
        good = link_raw_device(device, acsi_id)
        got_something = got_something or good   # if at least 1 device was symlinked, got_something will become true

    if got_something:               # if something was symlinked, the core should look for new IDs
        trigger_reload_raw()
