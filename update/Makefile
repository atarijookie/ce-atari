# needs mtools to write MS-DOS (FAT) file systems.

# -p to preserve files time stamps
CP = cp -p
ZIP = zip
GZCAT = gunzip -c


# config drive files :

TOOLS = configdrive/ce_conf.tos \
	configdrive/ce_fdd.prg \
	configdrive/ce_fdd.ttp \
	configdrive/ce_mount.tos

DRIVERS = configdrive/drivers/ce_cast.prg \
	configdrive/drivers/ce_dd.bs \
	configdrive/drivers/ce_dd_l2.bs \
	configdrive/drivers/ce_dd.prg \
	configdrive/drivers/ce_sting.prg \

TESTS = configdrive/tests/ce_tstfd.tos \
	configdrive/tests/ce_tsthd.tos \
	configdrive/tests/tst_fsys.tos

OTHERS = configdrive/readme.txt


# shell scripts :

SHELLSCRIPTS = \
shellscripts/ceboot.sh \
shellscripts/cesuper.sh \
shellscripts/ce_conf.sh \
shellscripts/ce_firstfw.sh \
shellscripts/ce_start.sh \
shellscripts/ce_stop.sh \
shellscripts/ce_update.sh \
shellscripts/copynewscripts.sh \
shellscripts/initd_cosmosex \
shellscripts/test_xc9536xl.xsvf \
shellscripts/test_xc9572xl.xsvf \
shellscripts/update_app.sh \
shellscripts/update_franz.sh \
shellscripts/update_hans.sh \
shellscripts/update_xilinx.sh \
shellscripts/wifisuper.sh


# WEBROOT

WEBROOTFILES = \
webroot/config.html \
webroot/credits.html \
webroot/css/bootstrap.light.css \
webroot/css/combined.css.gz \
webroot/css/style.css \
webroot/floppy.html \
webroot/img/260520st.jpg \
webroot/img/35dd.jpg \
webroot/img/ce_logo.png \
webroot/img/Scancode.gif \
webroot/index.html \
webroot/js/actual.js \
webroot/js/atariscreen.min.js \
webroot/js/config.js \
webroot/js/floppy.js \
webroot/js/jquery-combined.js.gz \
webroot/js/jquery.doubletap.js \
webroot/js/jquery.js \
webroot/js/keycode.js \
webroot/js/keylinux.js \
webroot/js/log.js \
webroot/js/modernizr.custom.50426.js \
webroot/js/screen.js \
webroot/js/script.js \
webroot/js/term.js \
webroot/js/zepto-combined.js.gz \
webroot/js/zepto.hack.js \
webroot/js/zepto.js \
webroot/remote.html \
webroot/screen.html \
webroot/screenshots.html \
webroot/templates/debug.tpl \
webroot/templates/layout.tpl \
webroot/templates/status.tpl


# CE main App

APPFILES = cosmosex cosmosex_raspbian cosmosex_yocto


#  make targets

.PHONY:	all clean

all:	app.zip

app.zip:	ce_conf.st ce_conf.zip fdd_test.st \
	$(TOOLS) $(DRIVERS) $(TESTS) $(OTHERS) \
	$(SHELLSCRIPTS) $(WEBROOTFILES) $(APPFILES)
	$(RM) $@
	$(ZIP) $@ $^

ce_conf.st:	$(DRIVERS) $(TOOLS) $(TESTS) $(OTHERS)
	$(RM) $@
	$(GZCAT) blank.st.gz > $@
	MTOOLS_NO_VFAT=1 mmd -i $@ ::DRIVERS
	MTOOLS_NO_VFAT=1 mmd -i $@ ::TESTS
	MTOOLS_NO_VFAT=1 mcopy -i $@ -v $(TOOLS) ::
	MTOOLS_NO_VFAT=1 mcopy -i $@ -v $(OTHERS) ::
	MTOOLS_NO_VFAT=1 mcopy -i $@ -v $(DRIVERS) ::/DRIVERS
	MTOOLS_NO_VFAT=1 mcopy -i $@ -v $(TESTS) ::/TESTS

fdd_test.st:	fdd_test.st.gz
	$(GZCAT) $< > $@

# zip options :
# -k : MSDOS compatible names and attributes
ce_conf.zip:	$(DRIVERS) $(TOOLS) $(TESTS) $(OTHERS)
	$(RM) $@
	cd configdrive ; $(ZIP) -k ../$@ $(patsubst configdrive/%,%,$^)

clean:
	$(RM) -r configdrive webroot shellscripts
	$(RM) *.zip *.st $(APPFILES)

configdrive/ce_conf.tos:	../ce_conf/ce_conf.tos
	mkdir -p configdrive
	$(CP) $< $@

configdrive/ce_fdd.prg:	../ce_fdd_prg/ce_fdd.prg
	mkdir -p configdrive
	$(CP) $< $@

configdrive/ce_fdd.ttp:	../ce_fdd_ttp/ce_fdd.ttp
	mkdir -p configdrive
	$(CP) $< $@

configdrive/ce_mount.tos:	../ce_mount_prg/ce_mount.tos
	mkdir -p configdrive
	$(CP) $< $@

configdrive/drivers/ce_cast.prg:	../ce_screencast/ce_cast.prg
	mkdir -p configdrive/drivers
	$(CP) $< $@

configdrive/drivers/%.bs:	../ce_dd_bootsectors/%.bs
	mkdir -p configdrive/drivers
	$(CP) $< $@

configdrive/drivers/ce_dd.prg:	../ce_dd_prg/ce_dd.prg
	mkdir -p configdrive/drivers
	$(CP) $< $@

configdrive/drivers/ce_sting.prg:	../cosmosex_fakesting/ce_sting.prg
	mkdir -p configdrive/drivers
	$(CP) $< $@

configdrive/tests/ce_tstfd.tos:	../test_floppy/ce_tstfd.tos
	mkdir -p configdrive/tests
	$(CP) $< $@

configdrive/tests/ce_tsthd.tos:	../test_acsi/ce_tsthd.tos
	mkdir -p configdrive/tests
	$(CP) $< $@

configdrive/tests/tst_fsys.tos:	../test_filesystem/tst_fsys.tos
	mkdir -p configdrive/tests
	$(CP) $< $@

configdrive/readme.txt:	../ce_main_app/configdrive/readme.txt
	mkdir -p configdrive
	$(CP) $< $@

shellscripts/%:	../ce_main_app/shellscripts/%
	mkdir -p shellscripts
	$(CP) $< $@

webroot/%:	../ce_main_app/webroot/%
	mkdir -p $(shell dirname $@)
	$(CP) $< $@

cosmosex:	../ce_main_app/shellscripts/ce_rename_for_distro.sh
	$(CP) $< $@

cosmosex_raspbian:	../ce_main_app/cosmosex_raspbian
	$(CP) $< $@

cosmosex_yocto:	../ce_main_app/cosmosex_yocto
	$(CP) $< $@
