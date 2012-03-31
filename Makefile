TARGET = particle
OBJS = main.o module/exception.o

INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = -fno-exceptions -fno-rtti
ASFLAGS = 

LIBDIR =
LDFLAGS =

BUILD_PRX = 1

LIBS = lib/dxlibp.a -lpspgu -lz -lm -lpsprtc -lpspaudio -lpspaudiocodec -lstdc++ -lpsputility -lpspvalloc -lpsppower

EXTRA_TARGETS = ./$(TARGET)/EBOOT.PBP
PSP_EBOOT = $(EXTRA_TARGETS)
PSP_EBOOT_TITLE = Particle

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

main.o: $(TARGET)/exception.prx
$(TARGET)/exception.prx:
	$(MKDIR) $(TARGET)
	$(CP) lib/exception.prx $(TARGET)
