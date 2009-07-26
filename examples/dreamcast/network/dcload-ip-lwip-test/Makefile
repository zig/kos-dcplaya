TARGET = dcload-lwip.elf

OBJS = main.o

LWIPDIR = $(KOS_BASE)/../kos-ports/lwip/lwip/src
ARCHDIR = $(LWIPDIR)/../../kos

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

KOS_CFLAGS += -DIPv4 \
	-I$(LWIPDIR)/include -I$(ARCHDIR)/include \
	-I$(LWIPDIR)/include/ipv4
#KOS_CFLAGS += -DIPv4 -DLWIP_DEBUG \
#	-I$(LWIPDIR)/include -I$(LWIPDIR)/arch/$(LWIPARCH)/include \
#	-I$(LWIPDIR)/include/ipv4 -I$(LWIPDIR)/..

clean:
	-rm -f $(TARGET) $(OBJS) romdisk.*

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $(TARGET) $(KOS_START) \
		$(OBJS) $(OBJEXTRA) -llwip4 $(KOS_LIBS)

dist:
	rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)
