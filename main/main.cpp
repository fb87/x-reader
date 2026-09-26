#include "esp_log.h"

namespace xreader
{
namespace app
{

static const char* const tag = "xreader";

static void run()
{
    ESP_LOGI(tag, "xreader baseline booted");
}

} // namespace app
} // namespace xreader

extern "C" void app_main(void)
{
    xreader::app::run();
}
