
### Variables

OBJECTS = main.o sliders.o

### Phony Targets

.PHONY: all
all: tweakable-bell

.PHONY: clean
clean:
	rm -f $(OBJECTS) tweakable-bell

.PHONY: run
run: tweakable-bell
	./tweakable-bell

### Actual Targets

tweakable-bell: $(OBJECTS)
	$(CC) -o $@ $^

### Dependencies

main.o: main.c main.h sliders.h
sliders.o: sliders.c sliders.h
