idf_component_register(
    SRC_DIRS  "." "./zigbee" "./ext" INCLUDE_DIRS "." "./zigbee" "./ext"
    PRIV_REQUIRES nvs_flash ieee802154 esp_timer app_update esp_delta_ota esp_adc esp_driver_pcnt i2cdev esp_driver_ledc
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DATE_CODE=\"${DATE_CODE}\"
    SW_VERSION=\"${SW_VERSION}\"
    FW_VERSION=${FW_VERSION}
)
