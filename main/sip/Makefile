
BUILD 	=	build
include $(BUILD)/Makefile.conf

ifeq ($(USE_DTLS),true)
BUILD_DTLS = dtls
endif

all: resiprocate dum tests 

resiprocate: contrib
	cd resiprocate; $(MAKE)

tests: resiprocate
	cd resiprocate/test; $(MAKE)

dum:	resiprocate
	cd resiprocate/dum; $(MAKE)

presSvr: resiprocate
	cd presSvr; $(MAKE)

contrib/ares/Makefile:
	cd contrib/ares && ./configure

configure_ares: contrib/ares/Makefile

ares: configure_ares
	cd contrib/ares && $(MAKE)

contrib/dtls/Makefile:
	cd contrib/dtls && ./config

configure_dtls: contrib/dtls/Makefile

dtls: configure_dtls
	cd contrib/dtls && $(MAKE)

contrib: ares $(BUILD_DTLS)

clean: 
	cd resiprocate; $(MAKE) clean
	cd resiprocate/test; $(MAKE) clean
	cd presSvr; $(MAKE) clean

.PHONY : resiprocate tests contrib ares dtls

