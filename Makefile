# global makefile


.PHONY: default ce_main_app atari_progs update check

default:
	@echo "usage :  make <atari_progs|ce_main_app|update>"

atari_progs:
	$(MAKE) -C ce_dd_bootsectors
	$(MAKE) -C ce_dd_prg
	$(MAKE) -C ce_screencast
	$(MAKE) -C cosmosex_fakesting
	$(MAKE) -C ce_conf
	$(MAKE) -C ce_fdd_prg
	$(MAKE) -C ce_fdd_ttp
	$(MAKE) -C ce_mount_prg
	$(MAKE) -C ce_mount_acc
	$(MAKE) -C ce_hdimg_ttp
	$(MAKE) -C test_floppy
	$(MAKE) -C test_floppy_read
	$(MAKE) -C test_filesystem
	$(MAKE) -C test_acsi
	$(MAKE) -C test_sting

ce_main_app:
	$(MAKE) -C ce_main_app

update:
	$(MAKE) -C update

check:
	$(MAKE) -C ce_main_app check
