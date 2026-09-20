#include "ui_retevis_rx.hpp"

#include "audio.hpp"
#include "baseband_api.hpp"
#include "memory_map.hpp"
#include "string_format.hpp"
#include "portapack.hpp"
#include "tonesets.hpp"
#include "dcs.hpp"

using namespace portapack;

namespace ui::external_app::retevis_rx {

const RetevisRxView::Channel RetevisRxView::channels_[16] = {
    {462562500, "TSQL", "67.0 Hz", 670, "023", "N", 0, false},
    {462587500, "TSQL", "118.8 Hz", 1188, "023", "N", 0, false},
    {462612500, "TSQL", "127.3 Hz", 1273, "023", "N", 0, false},
    {462637500, "TSQL", "131.8 Hz", 1318, "023", "N", 0, false},
    {462662500, "TSQL", "136.5 Hz", 1365, "023", "N", 0, false},
    {462625000, "TSQL", "127.3 Hz", 1273, "023", "N", 0, false},
    {462725000, "TSQL", "136.5 Hz", 1365, "023", "N", 0, false},
    {462687500, "TSQL", "141.3 Hz", 1413, "023", "N", 0, false},
    {462712500, "TSQL", "146.2 Hz", 1462, "023", "N", 0, false},
    {462550000, "TSQL", "123.0 Hz", 1230, "023", "N", 0, false},
    {462575000, "DTCS", "88.5 Hz", 0, "743", "RR", 0743, true},
    {462600000, "DTCS", "88.5 Hz", 0, "332", "RR", 0332, true},
    {462650000, "DTCS", "88.5 Hz", 0, "243", "RR", 0243, true},
    {462675000, "DTCS", "88.5 Hz", 0, "606", "NN", 0606, false},
    {462700000, "DTCS", "88.5 Hz", 0, "731", "RR", 0731, true},
    {462725000, "DTCS", "88.5 Hz", 0, "462", "RR", 0462, true},
};

RetevisRxView::RetevisRxView(NavigationView& nav)
    : nav_{nav} {
    field_channel.set_options({
        {"01 462.5625", 0}, {"02 462.5875", 1}, {"03 462.6125", 2}, {"04 462.6375", 3},
        {"05 462.6625", 4}, {"06 462.6250", 5}, {"07 462.7250", 6}, {"08 462.6875", 7},
        {"09 462.7125", 8}, {"10 462.5500", 9}, {"11 462.5750", 10}, {"12 462.6000", 11},
        {"13 462.6500", 12}, {"14 462.6750", 13}, {"15 462.7000", 14}, {"16 462.7250", 15},
    });
    channel_ = channel_ < 16 ? channel_ : 0;
    field_channel.set_by_value(static_cast<int32_t>(channel_));
    field_channel.on_change = [this](size_t, int32_t value) {
        select_channel(static_cast<uint32_t>(value));
    };

    field_squelch.set_value(static_cast<int32_t>(squelch_));
    field_squelch.on_change = [this](int32_t value) {
        squelch_ = static_cast<uint32_t>(value);
        if (rx_enabled_) {
            const auto& channel = channels_[channel_];
            baseband::set_tonedetect_config(
                static_cast<uint8_t>(squelch_),
                channel.tone_x10,
                channel.dtcs_code > 0 ? dcs::dcs_word(channel.dtcs_code) : 0,
                channel.dtcs_reverse);
        }
    };

    button_rx.on_select = [this](Button&) {
        toggle_rx();
    };

    add_children({&rssi, &field_volume, &labels, &field_channel, &text_mode,
                  &field_squelch, &button_rx, &text_status});

    select_channel(channel_);
    start_rx();
}

RetevisRxView::~RetevisRxView() {
    if (transmitting_)
        stop_transmit();
    if (rx_enabled_)
        stop_rx();
}

void RetevisRxView::focus() {
    field_channel.focus();
}

void RetevisRxView::select_channel(uint32_t index) {
    if (index >= 16) return;
    channel_ = index;
    const auto& channel = channels_[channel_];
    if (transmitting_)
        transmitter_model.set_target_frequency(channel.frequency_hz);
    else
        receiver_model.set_target_frequency(channel.frequency_hz);
    if (rx_enabled_)
        baseband::set_tonedetect_config(
            static_cast<uint8_t>(squelch_),
            channel.tone_x10,
            channel.dtcs_code > 0 ? dcs::dcs_word(channel.dtcs_code) : 0,
            channel.dtcs_reverse);
    update_status();
}

void RetevisRxView::update_status() {
    const auto& channel = channels_[channel_];
    text_mode.set(std::string(channel.mode) + " " + channel.tone + "  Code " + channel.code + channel.polarity);
    text_status.set(transmitting_
                        ? "TX - hold center button to talk"
                        : channel.mode[0] == 'T'
                              ? (rx_enabled_ ? "RX ON - TSQL active" : "RX OFF")
                              : (rx_enabled_ ? "RX ON - DTCS " + std::string(channel.code) + channel.polarity : "RX OFF"));
}

void RetevisRxView::start_rx() {
    if (rx_enabled_ || transmitting_) return;

    baseband::run_prepared_image(portapack::memory::map::m4_code.base());
    audio::set_rate(audio::Rate::Hz_24000);
    audio::output::start();
    receiver_model.set_sampling_rate(3072000);
    receiver_model.set_baseband_bandwidth(1750000);
    receiver_model.enable();
    const auto& channel = channels_[channel_];
    baseband::set_tonedetect_config(
        static_cast<uint8_t>(squelch_),
        channel.tone_x10,
        channel.dtcs_code > 0 ? dcs::dcs_word(channel.dtcs_code) : 0,
        channel.dtcs_reverse);
    rx_enabled_ = true;
    button_rx.set_text("RX OFF");
    update_status();
}

void RetevisRxView::stop_rx() {
    if (!rx_enabled_) return;

    receiver_model.disable();
    baseband::shutdown();
    audio::output::stop();
    rx_enabled_ = false;
    button_rx.set_text("RX ON");
    update_status();
}

void RetevisRxView::start_transmit() {
    if (transmitting_) return;

    resume_rx_after_tx_ = rx_enabled_;
    stop_rx();
    baseband::run_image(portapack::spi_flash::image_tag_mic_tx);
    audio::input::start(0, false);
    const auto& channel = channels_[channel_];
    const uint32_t tone_delta = channel.tone_x10 > 0
                                    ? TONES_F2D(channel.tone_x10 / 10.0f, TONES_SAMPLERATE)
                                    : 0;
    const uint32_t dtcs_word = channel.dtcs_code > 0 ? dcs::dcs_word(channel.dtcs_code) : 0;
    baseband::set_audiotx_config(
        1536000 / 20,
        5000.0f,
        1.0f,
        8,
        8,
        tone_delta,
        false,
        false,
        false,
        false,
        dtcs_word,
        channel.dtcs_reverse);
    transmitter_model.set_target_frequency(channel.frequency_hz);
    transmitter_model.set_sampling_rate(1536000);
    transmitter_model.set_baseband_bandwidth(1750000);
    transmitter_model.enable();
    transmitting_ = true;
    button_rx.set_text("TX");
    update_status();
}

void RetevisRxView::stop_transmit() {
    if (!transmitting_) return;

    audio::input::stop();
    transmitter_model.disable();
    baseband::shutdown();
    transmitting_ = false;
    if (resume_rx_after_tx_) {
        resume_rx_after_tx_ = false;
        start_rx();
    } else {
        button_rx.set_text("RX ON");
        update_status();
    }
}

void RetevisRxView::toggle_rx() {
    if (transmitting_) return;
    if (rx_enabled_)
        stop_rx();
    else
        start_rx();
}

void RetevisRxView::on_framesync() {
    const bool ptt_pressed = get_switches_state()[(size_t)ui::KeyEvent::Select];
    if (ptt_pressed && !ptt_was_pressed_)
        start_transmit();
    else if (!ptt_pressed && ptt_was_pressed_)
        stop_transmit();
    ptt_was_pressed_ = ptt_pressed;
}

}  // namespace ui::external_app::retevis_rx
