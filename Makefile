
### Variables

OBJECTS = main.o sliders.o bell.o

CFLAGS = -g -std=c99 -Os

ARCHES = x86_64

### Paths

PABLIO_ROOT ?= ../pablio

### Libraries and Frameworks

LIBRARIES += pablio portaudio
FRAMEWORKS += CoreAudio AudioToolbox

### Other

CFLAGS += $(foreach arch,$(ARCHES),-arch $(arch))
LDFLAGS += $(foreach arch,$(ARCHES),-arch $(arch))

CFLAGS += -I$(PABLIO_ROOT)/include
LDFLAGS += -L$(PABLIO_ROOT)/lib

### Phony Targets

.PHONY: all
all: tweakable-bell

.PHONY: clean
clean:
	rm -f $(OBJECTS) tweakable-bell *~

.PHONY: run
run: tweakable-bell
	./tweakable-bell

### Actual Targets

tweakable-bell: $(OBJECTS)
	$(CC) $(LDFLAGS) $(foreach lib,$(LIBRARIES),-l$(lib)) $(foreach fwk,$(FRAMEWORKS),-framework $(fwk)) -o $@ $^

### Dependencies

main.o: main.c main.h sliders.h bell.h
sliders.o: sliders.c sliders.h
bell.o: bell.c bell.h