#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_retevis_rx.hpp"
#include "external_app.hpp"

namespace ui::external_app::retevis_rx {
void initialize_app(ui::NavigationView& nav) {
    nav.push<RetevisRxView>();
}
}  // namespace ui::external_app::retevis_rx

extern "C" {

__attribute__((section(".external_app.app_retevis_rx.application_information"), used)) application_information_t _application_information_retevis_rx = {
    (uint8_t*)0x00000000,
    ui::external_app::retevis_rx::initialize_app,
    CURRENT_HEADER_VERSION,
    VERSION_MD5,
    "Retevis RX",
    {
        0x00, 0x00, 0x00, 0x00,
        0x18, 0x18, 0x3C, 0x3C,
        0x7E, 0x7E, 0xDB, 0xDB,
        0xFF, 0xFF, 0x18, 0x18,
        0x18, 0x18, 0x18, 0x18,
        0x3C, 0x3C, 0x66, 0x66,
        0xC3, 0xC3, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    ui::Color::green().v,
    app_location_t::RX,
    -1,
    {'P', 'T', 'N', 'E'},
    0x00000000,
};

}  // extern "C"
