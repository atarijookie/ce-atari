Welcome to the CosmosEx config drive!

Here you'll find all the tools you need 
to use the advanced features of your 
CosmosEx (meaning: features besides 
basic HDD functionality).

Please make sure you have read the 
manual (http://joo.kie.sk/?page_id=415). 

For a Quickstart guide, see 
http://joo.kie.sk/?page_id=704. 

Find more in-depth info in DrCoolZics 
docs at 
http://joo.kie.sk/cosmosex/download/
CosmosEx_Users_Guide.pdf

Also please visit your CosmosEx Web-
interface on it's configured network 
address (see your DHCP server for that 
or configure a static IP) for more 
confortable configuration and more 
infos.

This drive contains the following tools:


General
-------
ce_conf.tos  - Configure your CosmosEx

ce_fdd.prg   - Download and mount ST 
               files (Floppy Disk 
               Images)

ce_fdd.ttp   - Install this application
               for MSA or ST extensions 
               (Floppy Disk Images) on 
               your ST to automaticly 
               mount these into your 
               Floppy Disk Emulation.

ce_hdimg.ttp - Mount Hard Disk Image 
               Files with this tool - 
               e.g. backups from your 
               old harddisks


Folder DRIVERS\
---------------
(only needed for advanced CosmosEx 
 functionality, NOT for basic HDD usage)

ce_dd.prg - You only need this in 
            your AUTO folder if you 
            boot from another 
            harddrive or the CosmosEx
            SD card. By default 
            CosmosEx boots a builtin 
            version of this - so 
            normally you don't need 
            to touch it. CE_DD 
            (builtin and standalone)
            handles the more advanced
            features of your CosmosEx 
            like accessing thumb drives, 
            making screenshots and 
            keeping the current date - 
            it's kind of a supercharger
            for TOS.
            Neither version is needed 
            if you only want to use the 
            ATARI formatted SD card and 
            none of the other features.


ce_sting.prg - Highly experimental STiNG
               replacement for net-
               working. Not needed for 
               mounting networked drives
               (see CE_DD.PRG for that).

ce_cast.prg  - Screencast your STs desk-
               top to your web browser 
               and remote control it


Folder TESTS\ 
-------------
(Test tools for debugging issues that 
 may arise)

ce_tsthd.tos - Tests ACSI/SCSI communi-
               cation with several tests
               (Stresstests, R/W
               checksums etc.)

ce_tstfd.tos - Tests Floppy Disk Emu-
               lation

tst_fsys.tos - Tests File System (mainly
               Translated Drives)
