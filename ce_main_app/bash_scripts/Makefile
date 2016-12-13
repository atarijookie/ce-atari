SCRIPTS = $(wildcard *.sh)
SCRIPTS += initd_cosmosex
SCRIPTS += $(wildcard *.xsvf)

all:	../newscripts_zip.h

clean:
	$(RM) test.zip *.o test_zip

test:	test.zip
	cmp test.zip newscripts.zip

test.zip:	test_zip
	./$<

../newscripts_zip.h:	newscripts.zip
	xxd -i $^ > $@

newscripts.zip:	$(SCRIPTS)
	$(RM) $@
	zip $@ $^

test_zip:	test_zip.o

test_zip.o:	test_zip.c ../newscripts_zip.h
