all: hub-radio sensor-ht

# Sourcefiles linked with hub-radio
PROJECT_SOURCEFILES+= hub-fw-upload.c

# Sourcefiles linked with all sensor types
PROJECT_SOURCEFILES+= sensor-fw-download.c

# Sourcefiles linked with both hub-radio & all sensor types
PROJECT_SOURCEFILES+= util.c

TARGET=srf06-cc26xx
BOARD=veriradio/cc2650
CPU_FAMILY=cc26xx
CONTIKI=../contiki

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\" 
# CFLAGS += --save-temps

MODULES+=core/net/llsec/noncoresec

ifdef WITH_COMPOWER
APPS+=powertrace
CFLAGS+= -DCONTIKIMAC_CONF_COMPOWER=1 -DWITH_COMPOWER=1 -DQUEUEBUF_CONF_NUM=4
endif

ifdef SERVER_REPLY
CFLAGS+=-DSERVER_REPLY=$(SERVER_REPLY)
endif
ifdef PERIOD
CFLAGS+=-DPERIOD=$(PERIOD)
endif

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
