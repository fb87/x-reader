# X-Reader — TODO / Incomplete Items

This checklist tracks the remaining work needed to move X-Reader from the current functional prototype/framework into a complete, production-ready e-reader.

## P0 — Hardware / Core Platform

- [ ] Complete XTeink X4 SSD1677 display backend
- [ ] Add shared SPI-bus arbitration between e-paper and microSD
- [ ] Add build-time board selection
- [ ] Calibrate XTeink X4 ADC button thresholds on real hardware
- [ ] Verify physical button repeat / long-press behavior
- [ ] Implement power button, sleep, and wake flow
- [ ] Validate 800×480 layout and orientation on XTeink X4
- [x] Validate portrait/landscape touch transform on current touch device
- [ ] Add ESP-IDF target CI build for all supported boards

## P0 — Responsiveness / E-Paper Performance

- [ ] Measure complete latency path: input → queue → dispatch → render → upload → waveform complete
- [ ] Use fast waveform for normal reader page turns where supported
- [ ] Use partial DU refresh for focus/menu changes
- [ ] Add periodic GC16 cleanup refresh
- [ ] Merge overlapping dirty regions
- [ ] Avoid full-screen redraw on focus-only changes
- [ ] Confirm SPI burst-transfer performance on hardware
- [ ] Add per-stage performance logs behind diagnostics option

## P0 — Wi-Fi / Connectivity

- [x] Wi-Fi station mode
- [x] Saved SSID/password in NVS
- [x] Wi-Fi scan backend
- [x] Network list UI
- [x] Hidden-network flow
- [x] QWERTY/T9 text entry
- [x] Show explicit CONNECTING state
- [x] Show CONNECTED result and IP/RSSI
- [x] Detect and display AUTH FAILED / wrong password
- [x] Detect and display connection timeout
- [x] Add retry/cancel flow
- [x] Automatically return to Connectivity after successful pairing
- [x] Add forget-network confirmation
- [x] Add periodic RSSI refresh
- [x] Add rescan action
- [x] Automatically reconnect saved network at boot

## P1 — Keyboard / Text Input

- [x] Reusable QWERTY keyboard
- [x] Reusable T9 keyboard
- [x] Physical-button navigation
- [x] Touch navigation
- [x] Add symbols/special-character page
- [x] Add cursor movement
- [x] Add editing inside existing text
- [x] Add password show/hide toggle
- [x] Add physical-key repeat
- [x] Improve T9 commit timeout behavior
- [ ] Support long press where useful
- [x] Add reusable text-input dialog API
- [x] Use same input API for search, rename, Wi-Fi, sync URL, etc.

## P0 — Book Sync

- [x] Sync-service configuration
- [x] Reading-position upload
- [x] Define remote library manifest protocol
- [x] Fetch remote library manifest
- [x] Compare local vs remote books
- [x] Download new EPUB files
- [x] Update changed EPUB files
- [x] Support resumable `.part` downloads
- [x] Verify downloaded files before install
- [x] Download reading progress from server
- [x] Resolve reading-position conflicts
- [x] Sync bookmarks
- [x] Add offline retry queue
- [x] Add sync progress UI
- [x] Add sync history / last-error details
- [x] Define deletion/conflict policy

## P1 — Book Manager

- [x] Library browsing
- [x] Basic import flow
- [x] Temporary-file cleanup
- [x] Add persistent library index/database
- [x] Avoid full SD scan on normal startup
- [x] Extract EPUB title/author metadata
- [ ] Extract and cache cover image
- [x] Sort by title
- [x] Sort by author
- [x] Sort by recently added
- [x] Sort by recently read
- [x] Filter/search library
- [x] Rename book
- [x] Delete book with confirmation
- [x] Duplicate detection
- [x] Storage usage screen
- [ ] Handle SD removal/reinsert cleanly

## P1 — Reader

- [x] Responsive reader layout
- [x] Portrait/landscape mode
- [x] Physical-button page navigation
- [x] Touch page navigation
- [x] Table of contents
- [x] Book info
- [x] Session bookmarks
- [x] Persist bookmarks per book
- [x] Persist reading progress per book
- [x] Add reading history
- [x] Search inside current book
- [ ] Font selection
- [ ] Margin controls
- [ ] Text alignment controls
- [ ] Paragraph spacing controls
- [ ] Better chapter navigation
- [ ] Internal hyperlink navigation
- [ ] Footnote handling
- [ ] Image rendering inside EPUB
- [ ] Improve CSS support
- [x] Add progress indicator / percentage
- [ ] Add configurable tap zones / button mappings

## P1 — EPUB Compatibility

- [x] Normalize text to NFC at the document/text boundary
- [x] Keep lightweight Vietnamese NFD fallback
- [ ] Improve XHTML parser robustness
- [x] Improve HTML entity handling
- [ ] Add CSS subset parser
- [ ] Support EPUB2 NCX navigation
- [ ] Support EPUB3 nav document
- [ ] Recover gracefully from malformed EPUBs
- [ ] Handle very large chapters with streaming
- [ ] Avoid loading whole book/chapter when unnecessary
- [ ] Test Vietnamese-heavy EPUBs
- [ ] Test mixed Latin / CJK / symbol content
- [ ] Add compatibility regression corpus

## P1 — OTA / Firmware Update

- [x] OTA manifest lookup
- [x] Firmware download/install path
- [x] Proper semantic version comparison
- [x] Show download progress
- [x] Add user confirmation before install
- [x] Add low-battery guard
- [x] Add network-loss recovery
- [x] Add rollback support
- [ ] Add firmware verification/signature validation
- [ ] Add failed-update recovery UI
- [x] Display release notes
- [x] Record last OTA result

## P1 — Power Management

- [x] Configurable sleep timeout
- [x] Implement idle sleep
- [x] Implement deep sleep where appropriate
- [x] Wake from physical button (M5Paper centre/power key)
- [ ] Wake from touch on supported board
- [x] Persist current book/page before sleep
- [x] Shut down Wi-Fi before deep sleep
- [x] Reconnect Wi-Fi after wake only when needed
- [x] Read and display battery level (M5Paper ADC; XTeink hardware hook remains)
- [x] Low-battery warning
- [x] Critical-battery safe shutdown
- [ ] Measure idle/current consumption

## P1 — Remaining Screens

- [x] Home
- [x] Library
- [x] Reader
- [x] Reader quick menu
- [x] Table of contents
- [x] Bookmarks
- [x] Book info
- [x] Settings
- [x] Connectivity
- [x] OTA
- [x] Book Manager
- [x] Book Sync
- [x] Wi-Fi Networks
- [x] QWERTY/T9 Keyboard
- [x] Full File Browser
- [x] Rich Book Details screen
- [x] Search screen
- [x] Storage screen
- [x] Device/About screen
- [ ] Date/time settings
- [ ] Wi-Fi network details
- [x] Sync progress/history screen
- [x] OTA progress/result screen
- [x] Generic confirmation dialog
- [x] Generic warning/error dialog
- [ ] Empty-state screens
- [ ] First-run/onboarding screen

## P1 — Responsive GUI / Accessibility

- [x] Device-independent semantic actions
- [x] Physical-button focus navigation
- [x] Touch navigation
- [x] Responsive display classes
- [x] Portrait/landscape support
- [x] Touch-friendly bottom bar
- [ ] Ensure every interactive screen is fully button-navigable
- [ ] Ensure focus is always visible
- [ ] Add wrap/no-wrap focus policy per screen
- [ ] Preserve focus when returning from child screens
- [ ] Add minimum touch-target checks
- [ ] Add layout tests for 800×480
- [ ] Add layout tests for 960×540
- [ ] Add portrait layout regression tests
- [ ] Audit all remaining hard-coded coordinates

## P2 — Persistence / Reliability

- [x] Persist bookmarks
- [x] Persist recent books/history
- [x] Persist per-book progress
- [x] Persist UI preferences
- [x] Version NVS schema
- [ ] Handle corrupted NVS safely
- [ ] Handle corrupted EPUB safely
- [ ] Add filesystem error handling
- [ ] Add graceful SD-card failure handling
- [ ] Add watchdog-safe long operations
- [ ] Add crash recovery / last-session restore

## P2 — Testing / Production Readiness

- [x] Host unit tests
- [ ] ESP-IDF build test in CI
- [ ] Build all board profiles in CI
- [ ] Hardware-in-loop smoke test
- [ ] Input navigation tests
- [ ] Wi-Fi pairing tests
- [ ] OTA tests
- [ ] Sync protocol tests
- [ ] Low-memory tests on ESP32-C3
- [ ] Long-book pagination stress test
- [ ] Rapid page-turn stress test
- [ ] SD-card removal tests
- [ ] Power-cycle recovery tests
- [ ] Logging levels and release logging policy
- [ ] Production configuration profile
- [ ] Release packaging/versioning

## Suggested Milestones

### Milestone 1 — XTeink X4 usable
- [ ] SSD1677 backend
- [ ] shared SPI bus
- [ ] physical buttons
- [ ] sleep/wake
- [ ] 800×480 validation

### Milestone 2 — Connectivity complete
- [ ] polished Wi-Fi pairing
- [ ] keyboard completion
- [ ] connection/error states

### Milestone 3 — Real synchronization
- [ ] remote manifest
- [ ] EPUB download/update
- [ ] progress download/upload
- [ ] bookmark sync

### Milestone 4 — Daily-reader quality
- [ ] persistent library index
- [ ] persistent bookmarks/progress
- [ ] reader search
- [ ] EPUB image/CSS improvements
- [ ] power management

### Milestone 5 — Production readiness
- [ ] OTA hardening
- [ ] CI for all boards
- [ ] HIL tests
- [ ] low-memory/stress testing
- [ ] recovery paths
