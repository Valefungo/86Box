idf_component_register(SRCS "esp_lcd_touch_st7123.c" INCLUDE_DIRS "include" REQUIRES "esp_lcd")

# See i2c_bus/CMakeLists.txt for why this is hardcoded instead of using
# cmake_utilities' cu_pkg_define_version().
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ESP_LCD_TOUCH_ST7123_VER_MAJOR=1
    ESP_LCD_TOUCH_ST7123_VER_MINOR=0
    ESP_LCD_TOUCH_ST7123_VER_PATCH=2
)
