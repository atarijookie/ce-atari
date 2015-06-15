SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -mpcrel -nostdlib -DSLIM
LDFLAGS	= -Wl,--traditional-format

$(TARGET): $(STARTUP) $(SSRC) 
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(CSRC) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

all: bs_st.prg bs_tt.prg bs_l2.prg
    
bs_st.prg: bootsector_acsi_st.s
	$(CC) $(CFLAGS) -o $@ bootsector_acsi_st.s $(LDFLAGS)
	$(STRIP) -s $@

bs_tt.prg: bootsector_scsi_tt.s
	$(CC) $(CFLAGS) -o $@ bootsector_scsi_tt.s $(LDFLAGS)
	$(STRIP) -s $@

bs_l2.prg: bootsector_level2.s
	$(CC) $(CFLAGS) -o $@ bootsector_level2.s $(LDFLAGS)
	$(STRIP) -s $@
    
clean:
	rm -f *.prg *~
