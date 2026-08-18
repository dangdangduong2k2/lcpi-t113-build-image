TARGET := lcpi-lvgl-test
LVGL_LIB ?= /usr/lib/liblvgl.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) main.c -o $@ $(LDFLAGS) $(LVGL_LIB) -ldrm -lm -lpthread

clean:
	rm -f $(TARGET)
