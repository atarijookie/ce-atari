#!/bin/sh

REPO_PATH='/shared/ce-atari'

cd $REPO_PATH
cp -f appviasock/appviasock.elf shellscripts/.env shellscripts/ce.sh \
  /ce/services

cd $REPO_PATH/shellscripts
cp -f ce_update.sh getdotenv.sh distro.sh check_for_update.sh check_for_update_usb.sh post_install.sh \
  flash_chips.sh flash_stm32.sh flash_xilinx.sh \
  /ce/update

cd $REPO_PATH
cp -f flash_stm32/flash_stm32.elf flash_stm32_2021/flash_stm32_2021.elf flash_xilinx/flash_xilinx.elf \
  /ce/update

cd $REPO_PATH
cp -R ce_conf_py ce_main_app/core ce_fdd_py ce_mounter_py task_queue webserver \
  /ce/services
