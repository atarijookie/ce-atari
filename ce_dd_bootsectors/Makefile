SLIM	= yes

CC	= m68k-atari-mint-gcc
STRIP	= m68k-atari-mint-strip
CFLAGS	= -O2 -m68000 -fomit-frame-pointer -Wall -Wno-multichar -mpcrel -nostdlib -DSLIM
LDFLAGS	= -Wl,--traditional-format

HOSTCXX = g++
CXXFLAGS = -Wall

$(TARGET): $(STARTUP) $(SSRC) 
	$(CC) $(CFLAGS) -o $@ $(STARTUP) $(CSRC) $(SSRC) $(LDFLAGS)
	$(STRIP) -s $@

all: ce_dd_st.bs ce_dd_tt.bs ce_dd_fn.bs ce_dd_l2.bs

convertor:	convertor.cpp
	$(HOSTCXX) $(CXXFLAGS) -o $@ $<

convertor:	bs_st.prg bs_tt.prg bs_fn.prg bs_l2.prg ../ce_dd_prg/ce_dd.prg

../ce_dd_prg/ce_dd.prg:	../ce_dd_prg/main.c
	$(MAKE) -C ../ce_dd_prg

ce_dd_%.bs:	bs_%.prg
	$(MAKE) convertor
	./convertor

bs_st.prg: bootsector_acsi_st.s
	$(CC) $(CFLAGS) -o $@ bootsector_acsi_st.s $(LDFLAGS)
	$(STRIP) -s $@

bs_tt.prg: bootsector_scsi_tt.s
	$(CC) $(CFLAGS) -o $@ bootsector_scsi_tt.s $(LDFLAGS)
	$(STRIP) -s $@

bs_fn.prg: bootsector_scsi_falcon.s
	$(CC) $(CFLAGS) -o $@ bootsector_scsi_falcon.s $(LDFLAGS)
	$(STRIP) -s $@
    
bs_l2.prg: bootsector_level2.s
	$(CC) $(CFLAGS) -o $@ bootsector_level2.s $(LDFLAGS)
	$(STRIP) -s $@
    
clean:
	$(RM) *.prg *.bs *~ *.o convertor
