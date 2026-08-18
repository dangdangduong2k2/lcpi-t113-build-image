# C-only LVGL build wrapper. The upstream CMake project unconditionally enables
# C++, while this test uses only LVGL's C framebuffer and evdev backends.

# Build the core plus the upstream benchmark and its widgets scene.  Other
# LVGL demos stay out of the bring-up image.
SRCS := demos/lv_demos.c $(shell find src demos/benchmark demos/widgets -type f -name '*.c' -print | LC_ALL=C sort)
OBJS := $(patsubst %.c,.build/%.o,$(SRCS))

override CPPFLAGS += -I. -DLV_CONF_INCLUDE_SIMPLE -DLV_KCONFIG_IGNORE
override CFLAGS += -Wall -Wextra -mfpu=neon-vfpv4

.PHONY: all

all: liblvgl.a

liblvgl.a: $(OBJS)
	$(AR) rcs $@ $^

.build/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
