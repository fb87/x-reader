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
- [ ] Validate portrait/landscape touch transform on current touch device
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
- [ ] Add retry/cancel flow
- [x] Automatically return to Connectivity after successful pairing
- [ ] Add forget-network confirmation
- [ ] Add periodic RSSI refresh
- [ ] Add rescan action
- [x] Automatically reconnect saved network at boot

## P1 — Keyboard / Text Input

- [x] Reusable QWERTY keyboard
- [x] Reusable T9 keyboard
- [x] Physical-button navigation
- [x] Touch navigation
- [ ] Add symbols/special-character page
- [ ] Add cursor movement
- [ ] Add editing inside existing text
- [ ] Add password show/hide toggle
- [ ] Add physical-key repeat
- [ ] Improve T9 commit timeout behavior
- [ ] Support long press where useful
- [ ] Add reusable text-input dialog API
- [ ] Use same input API for search, rename, Wi-Fi, sync URL, etc.

## P0 — Book Sync

- [x] Sync-service configuration
- [x] Reading-position upload
- [ ] Define remote library manifest protocol
- [ ] Fetch remote library manifest
- [ ] Compare local vs remote books
- [ ] Download new EPUB files
- [ ] Update changed EPUB files
- [ ] Support resumable `.part` downloads
- [ ] Verify downloaded files before install
- [ ] Download reading progress from server
- [ ] Resolve reading-position conflicts
- [ ] Sync bookmarks
- [ ] Add offline retry queue
- [ ] Add sync progress UI
- [ ] Add sync history / last-error details
- [ ] Define deletion/conflict policy

## P1 — Book Manager

- [x] Library browsing
- [x] Basic import flow
- [x] Temporary-file cleanup
- [ ] Add persistent library index/database
- [ ] Avoid full SD scan on normal startup
- [ ] Extract EPUB title/author metadata
- [ ] Extract and cache cover image
- [ ] Sort by title
- [ ] Sort by author
- [ ] Sort by recently added
- [ ] Sort by recently read
- [ ] Filter/search library
- [ ] Rename book
- [ ] Delete book with confirmation
- [ ] Duplicate detection
- [ ] Storage usage screen
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
- [ ] Add reading history
- [ ] Search inside current book
- [ ] Font selection
- [ ] Margin controls
- [ ] Text alignment controls
- [ ] Paragraph spacing controls
- [ ] Better chapter navigation
- [ ] Internal hyperlink navigation
- [ ] Footnote handling
- [ ] Image rendering inside EPUB
- [ ] Improve CSS support
- [ ] Add progress indicator / percentage
- [ ] Add configurable tap zones / button mappings

## P1 — EPUB Compatibility

- [ ] Normalize text to NFC at the document/text boundary
- [ ] Keep lightweight Vietnamese NFD fallback
- [ ] Improve XHTML parser robustness
- [ ] Improve HTML entity handling
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
- [ ] Proper semantic version comparison
- [ ] Show download progress
- [ ] Add user confirmation before install
- [ ] Add low-battery guard
- [ ] Add network-loss recovery
- [ ] Add rollback support
- [ ] Add firmware verification/signature validation
- [ ] Add failed-update recovery UI
- [ ] Display release notes
- [ ] Record last OTA result

## P1 — Power Management

- [x] Configurable sleep timeout
- [ ] Implement idle sleep
- [ ] Implement deep sleep where appropriate
- [ ] Wake from physical button
- [ ] Wake from touch on supported board
- [ ] Persist current book/page before sleep
- [ ] Shut down Wi-Fi before deep sleep
- [ ] Reconnect Wi-Fi after wake only when needed
- [ ] Read and display battery level
- [ ] Low-battery warning
- [ ] Critical-battery safe shutdown
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
- [ ] Full File Browser
- [ ] Rich Book Details screen
- [ ] Search screen
- [ ] Storage screen
- [ ] Device/About screen
- [ ] Date/time settings
- [ ] Wi-Fi network details
- [ ] Sync progress/history screen
- [ ] OTA progress/result screen
- [ ] Generic confirmation dialog
- [ ] Generic warning/error dialog
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
- [ ] Persist recent books/history
- [x] Persist per-book progress
- [ ] Persist UI preferences
- [ ] Version NVS schema
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
