// vim:tabstop=4 softtabstop=4 shiftwidth=4 expandtab colorcolumn=80

#include <M5Unified.h>

extern "C" void app_main(void) {
    auto cfg = M5.config();
    cfg.external_display.module_display = true;

    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.clear(TFT_WHITE);

    M5.Display.setFont(&fonts::FreeSans9pt7b);

    M5.Display.setTextSize(1.5);
    M5.Display.print("Featured books are quality books that the community believes to be the best of what Wikibooks has to offer and should inspire people to improve the quality of other books. \n\nTo nominate one of our Wikibooks, visit the Nominations page. All nominations should meet the minimum criteria to be featured.\n");
    M5.Display.print("Featured books are quality books that the community believes to be the best of what Wikibooks has to offer and should inspire people to improve the quality of other books. \n\nTo nominate one of our Wikibooks, visit the Nominations page. All nominations should meet the minimum criteria to be featured.\n");
    M5.Display.print("Featured books are quality books that the community believes to be the best of what Wikibooks has to offer and should inspire people to improve the quality of other books. \n\nTo nominate one of our Wikibooks, visit the Nominations page. All nominations should meet the minimum criteria to be featured.\n");
    M5.Display.print("Featured books are quality books that the community believes to be the best of what Wikibooks has to offer and should inspire people to improve the quality of other books. \n\nTo nominate one of our Wikibooks, visit the Nominations page. All nominations should meet the minimum criteria to be featured.\n");
}

