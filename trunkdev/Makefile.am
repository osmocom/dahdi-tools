CFLAGS += -g -Wall -O2 $(DAHDI_INCLUDE)

if PBX_TRUNKDEV

sbin_PROGRAMS = \
	dahdi_trunkdev \
	$(NULL)

dahdi_trunkdev_SOURCES = \
	trunkdev-tool.c \
	dahdi-sysfs.c \
	$(NULL)

endif
