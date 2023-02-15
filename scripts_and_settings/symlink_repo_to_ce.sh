#!/bin/sh

rm -f  /ce/services/appviasock*
rm -f  /ce/services/.env
rm -f  /ce/services/ce.sh
rm -rf /ce/services/config
rm -rf /ce/services/core
rm -rf /ce/services/floppy
rm -rf /ce/services/mounter
rm -rf /ce/services/taskq
rm -rf /ce/services/webserver
rm -f /ce/update/ce_update.sh
rm -f /ce/update/getdotenv.sh
rm -f /ce/update/distro.sh

ln -fs /shared/ce-atari/appviasock/appviasock.elf         /ce/services/appviasock.elf
ln -fs /shared/ce-atari/scripts_and_settings/.env         /ce/services/.env
ln -fs /shared/ce-atari/scripts_and_settings/ce.sh        /ce/services/ce.sh
ln -fs /shared/ce-atari/scripts_and_settings/ce_update.sh /ce/update/ce_update.sh
ln -fs /shared/ce-atari/scripts_and_settings/getdotenv.sh /ce/update/getdotenv.sh
ln -fs /shared/ce-atari/scripts_and_settings/distro.sh    /ce/update/distro.sh
ln -fs /shared/ce-atari/ce_conf_py                        /ce/services/config
ln -fs /shared/ce-atari/ce_main_app/core                  /ce/services/core
ln -fs /shared/ce-atari/ce_fdd_py                         /ce/services/floppy
ln -fs /shared/ce-atari/ce_mounter_py                     /ce/services/mounter
ln -fs /shared/ce-atari/task_queue                        /ce/services/taskq
ln -fs /shared/ce-atari/webserver                         /ce/services/webserver
