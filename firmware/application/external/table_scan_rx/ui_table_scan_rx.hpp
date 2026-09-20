#ifndef __UI_TABLE_SCAN_RX_H__
#define __UI_TABLE_SCAN_RX_H__

#include "app_settings.hpp"
#include "file.hpp"
#include "file_reader.hpp"
#include "message.hpp"
#include "radio_state.hpp"
#include "ui.hpp"
#include "ui_fileman.hpp"
#include "ui_freq_field.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ui::external_app::table_scan_rx {

class TableScanRxView : public View {
   public:
    explicit TableScanRxView(NavigationView& nav);
    ~TableScanRxView();

    void focus() override;
    std::string title() const override { return "CHIRP Table RX"; }

   private:
    static constexpr const char* table_filename = "CHIRP.TXT";

    struct Channel {
        rf::Frequency frequency;
        std::string name;
        uint32_t ctcss_x10{0};
        uint16_t dtcs_code{0};
        bool dtcs_reverse{false};
    };

    NavigationView& nav_;
    RxRadioState radio_state_{};
    std::vector<Channel> channels_{};
    uint32_t channel_{0};
    uint32_t squelch_{50};
    uint32_t dwell_frames_{0};
    bool running_{false};
    bool scanning_{false};
    bool detected_{false};

    app_settings::SettingsManager settings_{
        "rx_table_scan",
        app_settings::Mode::RX,
        {{"channel"sv, &channel_}, {"squelch"sv, &squelch_}}};

    RSSI rssi{{UI_POS_X(21), 0, UI_POS_WIDTH_REMAINING(24), 4}};
    AudioVolumeField field_volume{{UI_POS_X_RIGHT(2), UI_POS_Y(0)}};
    Text text_file{{0, 1 * 16, screen_width, 16}, "Load a table"};
    Text text_channel{{0, 2 * 16, screen_width, 16}, ""};
    Text text_status{{0, 3 * 16, screen_width, 16}, ""};
    Button button_load{{0, 4 * 16, 9 * 8, 18}, "Load"};
    Button button_scan{{10 * 8, 4 * 16, 9 * 8, 18}, "Scan"};
    NumberField field_squelch{{20 * 8, 4 * 16, 2 * 8, 18}, 2, {0, 99}, 1, ' '};
    OptionsField field_channel{{0, 5 * 16, 30 * 8, 18}, 29, {}};

    void load_table(const std::filesystem::path& path);
    void update_channel_options();
    void tune_channel(uint32_t index);
    void start_receiver();
    void stop_receiver();
    void start_scan();
    void stop_scan();
    void advance_channel();
    void on_framesync();
    void on_tone_data(const ToneDetectDataMessage* message);
    void update_status();

    MessageHandlerRegistration message_handler_framesync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) { on_framesync(); }};
    MessageHandlerRegistration message_handler_tone{
        Message::ID::ToneDetectData,
        [this](const Message* const p) {
            on_tone_data(reinterpret_cast<const ToneDetectDataMessage*>(p));
        }};
};

}  // namespace ui::external_app::table_scan_rx

#endif /* __UI_TABLE_SCAN_RX_H__ */
