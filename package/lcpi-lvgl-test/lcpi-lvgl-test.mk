################################################################################
#
# lcpi-lvgl-test
#
################################################################################

LCPI_LVGL_TEST_VERSION = 1.0
LCPI_LVGL_TEST_SITE = $(BR2_EXTERNAL_LCPI_T113_PATH)/package/lcpi-lvgl-test/src
LCPI_LVGL_TEST_SITE_METHOD = local
LCPI_LVGL_TEST_LICENSE = MIT
LCPI_LVGL_TEST_LICENSE_FILES = LICENSE
LCPI_LVGL_TEST_DEPENDENCIES = lvgl libdrm

define LCPI_LVGL_TEST_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CPPFLAGS="-I$(STAGING_DIR)/usr/include/lvgl -I$(STAGING_DIR)/usr/include/libdrm -DLV_CONF_INCLUDE_SIMPLE" \
		CFLAGS="$(TARGET_CFLAGS) -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -mfpu=neon-vfpv4" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		LVGL_LIB="$(STAGING_DIR)/usr/lib/liblvgl.a"
endef

define LCPI_LVGL_TEST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/lcpi-lvgl-test $(TARGET_DIR)/usr/bin/lcpi-lvgl-test
	$(INSTALL) -D -m 0755 $(LCPI_LVGL_TEST_PKGDIR)/S99lcpi-lvgl-test \
		$(TARGET_DIR)/etc/init.d/S99lcpi-lvgl-test
	$(INSTALL) -D -m 0755 $(LCPI_LVGL_TEST_PKGDIR)/S98lcpi-performance \
		$(TARGET_DIR)/etc/init.d/S98lcpi-performance
endef

$(eval $(generic-package))
