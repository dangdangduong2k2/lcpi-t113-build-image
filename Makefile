BUILDROOT_VERSION := 2026.05.1
BUILDROOT_SHA256 := ae7f706f087b9ae9083a10a587368dfbf53103c28bf81c2d690198dc4090cb58
BUILDROOT_ARCHIVE := buildroot-$(BUILDROOT_VERSION).tar.xz
BUILDROOT_URL := https://buildroot.org/downloads/$(BUILDROOT_ARCHIVE)

PROJECT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILDROOT_WORKDIR ?= $(HOME)/lcpi-t113-work
BUILDROOT_DIR := $(BUILDROOT_WORKDIR)/buildroot-$(BUILDROOT_VERSION)
BUILDROOT_TARBALL := $(BUILDROOT_WORKDIR)/downloads/$(BUILDROOT_ARCHIVE)
OUTPUT_DIR ?= $(BUILDROOT_WORKDIR)/output
LCPI_JOBS ?= $(shell nproc 2>/dev/null || echo 1)

# WSL normally appends the Windows PATH, which contains entries such as
# "Program Files". Buildroot rejects any PATH containing whitespace, so keep
# this project on a deterministic Linux-only host PATH.
LCPI_LINUX_PATH ?= /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH := $(LCPI_LINUX_PATH)

.PHONY: all help prepare ensure-config configure build menuconfig linux-menuconfig uboot-menuconfig rebuild-linux rebuild-uboot rebuild-lvgl-test rebuild-lvgl update-defconfig print-paths

all: build

help:
	@echo "LCPI-PC-T113 Buildroot project"
	@echo "  make configure        Reset output/.config to lcpi_t113_defconfig"
	@echo "  make build            Build sdcard.img (configures on first run)"
	@echo "  make menuconfig       Open Buildroot configuration"
	@echo "  make linux-menuconfig Open Linux kernel configuration"
	@echo "  make uboot-menuconfig Open U-Boot configuration"
	@echo "  make rebuild-lvgl-test Rebuild target LVGL application after editing package/lcpi-lvgl-test"
	@echo "  make rebuild-lvgl      Rebuild LVGL library and target app after editing package/lvgl/lv_conf.h"
	@echo "  make update-defconfig  Save the current menuconfig choices into configs/lcpi_t113_defconfig"
	@echo "  make print-paths      Show source, output, and image paths"

prepare:
	@command -v wget >/dev/null
	@command -v sha256sum >/dev/null
	@command -v tar >/dev/null
	@mkdir -p "$(BUILDROOT_WORKDIR)/downloads"
	@if [ ! -f "$(BUILDROOT_TARBALL)" ]; then \
		wget --https-only -O "$(BUILDROOT_TARBALL).tmp" "$(BUILDROOT_URL)"; \
		mv "$(BUILDROOT_TARBALL).tmp" "$(BUILDROOT_TARBALL)"; \
	fi
	@printf '%s  %s\n' "$(BUILDROOT_SHA256)" "$(BUILDROOT_TARBALL)" | sha256sum --check --status
	@if [ ! -f "$(BUILDROOT_DIR)/Makefile" ]; then \
		tar -xf "$(BUILDROOT_TARBALL)" -C "$(BUILDROOT_WORKDIR)"; \
	fi

configure: prepare
	@mkdir -p "$(OUTPUT_DIR)"
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" lcpi_t113_defconfig

ensure-config: prepare
	@mkdir -p "$(OUTPUT_DIR)"
	@if [ ! -f "$(OUTPUT_DIR)/.config" ]; then \
		$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" lcpi_t113_defconfig; \
	fi

build: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" -j"$(LCPI_JOBS)"
	@echo "Image: $(OUTPUT_DIR)/images/sdcard.img"

menuconfig: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" menuconfig

linux-menuconfig: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" linux-menuconfig

uboot-menuconfig: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" uboot-menuconfig

rebuild-linux: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" linux-rebuild all

rebuild-uboot: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" uboot-rebuild all

# LCPI_LVGL_TEST_SITE_METHOD=local is intentionally used for quick local edits,
# but Buildroot only re-syncs that package when its package rebuild target runs.
rebuild-lvgl-test: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" lcpi-lvgl-test-rebuild
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" -j"$(LCPI_JOBS)" all

# lv_conf.h is compiled into liblvgl.a, so rebuild the library and the app that
# links against it.  A normal `make build` may leave old local-package objects.
rebuild-lvgl: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" lvgl-rebuild
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" lcpi-lvgl-test-rebuild
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" -j"$(LCPI_JOBS)" all

update-defconfig: ensure-config
	$(MAKE) -C "$(BUILDROOT_DIR)" O="$(OUTPUT_DIR)" BR2_EXTERNAL="$(PROJECT_DIR)" savedefconfig
	cp "$(OUTPUT_DIR)/defconfig" "$(PROJECT_DIR)/configs/lcpi_t113_defconfig"

print-paths:
	@echo "Project:   $(PROJECT_DIR)"
	@echo "Buildroot: $(BUILDROOT_DIR)"
	@echo "Output:    $(OUTPUT_DIR)"
	@echo "Image:     $(OUTPUT_DIR)/images/sdcard.img"
