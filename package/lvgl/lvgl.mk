################################################################################
#
# lvgl
#
################################################################################

LVGL_VERSION = 9.5.0
LVGL_SITE = $(call github,lvgl,lvgl,v$(LVGL_VERSION))
LVGL_LICENSE = MIT
LVGL_LICENSE_FILES = LICENCE.txt
LVGL_DEPENDENCIES = libdrm
LVGL_INSTALL_STAGING = YES
LVGL_INSTALL_TARGET = NO

define LVGL_BUILD_CMDS
	cp $(LVGL_PKGDIR)/lv_conf.h $(@D)/lv_conf.h
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) -f $(LVGL_PKGDIR)/buildroot.make \
	CC="$(TARGET_CC)" \
	AR="$(TARGET_AR)" \
	CPPFLAGS="-I$(STAGING_DIR)/usr/include/libdrm" \
	CFLAGS="$(TARGET_CFLAGS) -std=gnu11 -ffunction-sections -fdata-sections"
endef

define LVGL_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/liblvgl.a $(STAGING_DIR)/usr/lib/liblvgl.a
	mkdir -p $(STAGING_DIR)/usr/include/lvgl
	cp -a $(@D)/src $(STAGING_DIR)/usr/include/lvgl/
	cp -a $(@D)/demos $(STAGING_DIR)/usr/include/lvgl/
	$(INSTALL) -D -m 0644 $(@D)/lvgl.h $(STAGING_DIR)/usr/include/lvgl/lvgl.h
	$(INSTALL) -D -m 0644 $(@D)/lv_version.h $(STAGING_DIR)/usr/include/lvgl/lv_version.h
	$(INSTALL) -D -m 0644 $(LVGL_PKGDIR)/lv_conf.h $(STAGING_DIR)/usr/include/lvgl/lv_conf.h
endef

$(eval $(generic-package))
