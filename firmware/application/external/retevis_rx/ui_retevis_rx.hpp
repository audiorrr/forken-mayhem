#ifndef __UI_RETEVIS_RX_H__
#define __UI_RETEVIS_RX_H__

#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "ui_freq_field.hpp"
#include "app_settings.hpp"
#include "radio_state.hpp"
#include "message.hpp"
#include "transmitter_model.hpp"

#include <cstdint>
#include <string>

namespace ui::external_app::retevis_rx {

class RetevisRxView : public View {
   public:
    explicit RetevisRxView(NavigationView& nav);
    ~RetevisRxView();

    void focus() override;
    std::string title() const override { return "Retevis RX"; }

   private:
    struct Channel {
        uint32_t frequency_hz;
        const char* mode;
        const char* tone;
        uint32_t tone_x10;
        const char* code;
        const char* polarity;
        uint16_t dtcs_code;
        bool dtcs_reverse;
    };

    static const Channel channels_[16];

    NavigationView& nav_;
    RxRadioState radio_state_{};
    TxRadioState tx_radio_state_{};
    uint32_t channel_{0};
    uint32_t squelch_{50};
    bool rx_enabled_{false};
    bool transmitting_{false};
    bool resume_rx_after_tx_{false};
    bool ptt_was_pressed_{false};

    app_settings::SettingsManager settings_{
        "rx_retevis",
        app_settings::Mode::RX_TX,
        {{"channel"sv, &channel_}, {"squelch"sv, &squelch_}}};

    RSSI rssi{{UI_POS_X(21), 0, UI_POS_WIDTH_REMAINING(24), 4}};
    AudioVolumeField field_volume{{UI_POS_X_RIGHT(2), UI_POS_Y(0)}};

    Labels labels{{
        {{0, 1 * 16}, "Channel:", Theme::getInstance()->fg_light->foreground},
        {{0, 2 * 16}, "Mode:", Theme::getInstance()->fg_light->foreground},
        {{0, 3 * 16}, "Squelch:", Theme::getInstance()->fg_light->foreground},
    }};
    OptionsField field_channel{{7 * 8, 1 * 16}, 22, {}};
    Text text_mode{{7 * 8, 2 * 16, 23 * 8, 16}, ""};
    NumberField field_squelch{{9 * 8, 3 * 16}, 2, {0, 99}, 1, ' '};
    Text text_status{{0, 4 * 16, screen_width, 16}, ""};
    Button button_rx{{25 * 8, 1 * 16, 5 * 8, 18}, "RX ON"};

    void select_channel(uint32_t index);
    void update_status();
    void start_rx();
    void stop_rx();
    void start_transmit();
    void stop_transmit();
    void toggle_rx();
    void on_framesync();

    MessageHandlerRegistration message_handler_framesync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) {
            this->on_framesync();
        }};
};

}  // namespace ui::external_app::retevis_rx

#endif /* __UI_RETEVIS_RX_H__ */
