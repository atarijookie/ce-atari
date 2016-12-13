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
ce_conf.prg  - Configure your CosmosEx

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

ce_dd.prg    - This driver is only 
               needed if you want to use
               translated drives (e.g. 
               windows thumb drives 
               emulated as TOS drives or 
               network drives), 
               screenshots and current 
               date. This is only needed
               if you don't boot from 
               CE_DD ACSI, then it loads 
               automaticly (see manual).

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

ce_tsthd.prg - Tests ACSI/SCSI communi-
               cation with several tests
               (Stresstests, R/W checks-
               ums etc.)

ce_tstfd.prg - Tests Floppy Disk Emu-
               lation

tst_fsys.prg - Tests File System (mainly
               Translated Drives)
