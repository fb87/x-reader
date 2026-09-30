#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "services/library_index.hpp"

namespace xreader
{
namespace services
{
namespace library_scan
{

// Import/cleanup themselves are cheap directory operations, but the catalog
// rebuild that must follow re-parses every EPUB's metadata from scratch and
// takes multiple seconds with a few dozen books.  Running it on the UI task
// dropped input for that whole time.  This service runs it on a background
// task instead; the UI polls busy()/take_result() the same way it already
// polls services::connectivity and services::ota.
enum action_t : uint8_t
{
    action_import,
    action_cleanup,
    action_rescan,
};

struct result_t
{
    uint16_t imported;
    uint16_t removed;
    esp_err_t error;
};

// Fails with ESP_ERR_INVALID_STATE if a scan is already running.
esp_err_t request(action_t action, const char* mount_path);
bool busy();

// Copies the freshly rebuilt catalog into *catalog and the result into
// *result, then clears the ready flag.  Returns false (leaving both
// untouched) when no result is waiting.  Call from the UI task only, once per
// completed scan, since the copy is a few tens of KB.
bool take_result(library_index::catalog_t* catalog, result_t* result);

} // namespace library_scan
} // namespace services
} // namespace xreader
