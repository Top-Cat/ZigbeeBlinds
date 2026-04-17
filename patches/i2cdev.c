--- managed_components/esp-idf-lib__i2cdev/i2cdev.c	2026-04-17 17:05:14
+++ managed_components/esp-idf-lib__i2cdev/i2cdev.c	2026-04-17 17:04:42
@@ -106,13 +106,14 @@
         }
     }
 }
+
+static bool initialized = false;
 
 esp_err_t i2cdev_init(void)
 {
     ESP_LOGV(TAG, "Initializing I2C subsystem...");
 
     // Guard against re-initialization to prevent resource leaks
-    static bool initialized = false;
     if (initialized)
     {
         ESP_LOGW(TAG, "I2C subsystem already initialized, skipping re-initialization");
@@ -938,6 +939,7 @@
 
         } // end if lock exists
     } // end for loop
+    initialized = false;
     ESP_LOGV(TAG, "I2C subsystem cleanup finished with result: %d", result);
     return result;
 }
