# Sync and OTA reliability

## Book sync retry

A sync request is persisted before the worker starts. If any library, progress, bookmark, or HTTP step fails, the current book/spine/page request remains in NVS and is retried automatically after connectivity returns. Retries are rate-limited to one attempt every 30 seconds. Successful sync clears the pending request.

The service keeps the four most recent result messages in NVS. The Book Sync screen shows pending retry state, transfer progress, the last result, and recent history.

## OTA progress and recovery

Firmware installation reports byte and percentage progress from the HTTP transfer callback. A failed transfer is retried up to three times, allowing transient network interruptions to recover without user intervention. The final check/install result is persisted in NVS and displayed on the System Update screen after reboot.

When ESP-IDF rollback support is enabled, a newly booted image remains pending verification until X-Reader has initialized the display and rendered the Home screen. It is then marked valid. This leaves the bootloader rollback window open across early-startup failures.

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` is included in `sdkconfig.defaults`.

Firmware signature enforcement still depends on the ESP-IDF secure-boot/signing configuration and remains a separate production-hardening task.
