#include "external_app.hpp"
#include "ui_navigation.hpp"
#include "ui_table_scan_rx.hpp"

namespace ui::external_app::table_scan_rx {
void initialize_app(ui::NavigationView& nav) {
    nav.push<TableScanRxView>();
}
}  // namespace ui::external_app::table_scan_rx

extern "C" {

__attribute__((section(".external_app.app_table_scan_rx.application_information"), used)) application_information_t _application_information_table_scan_rx = {
    (uint8_t*)0x00000000,
    ui::external_app::table_scan_rx::initialize_app,
    CURRENT_HEADER_VERSION,
    VERSION_MD5,
    "CHIRP Table RX",
    {
        0x00, 0x00, 0x00, 0x00,
        0x18, 0x18, 0x18, 0x18,
        0x3C, 0x3C, 0x7E, 0x7E,
        0xFF, 0xFF, 0x7E, 0x7E,
        0x3C, 0x3C, 0x18, 0x18,
        0x18, 0x18, 0x18, 0x18,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    ui::Color::cyan().v,
    app_location_t::RX,
    -1,
    {'P', 'T', 'N', 'E'},
    0x00000000,
};

}  // extern "C"
