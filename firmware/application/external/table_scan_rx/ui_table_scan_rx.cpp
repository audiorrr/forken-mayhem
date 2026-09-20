#include "ui_table_scan_rx.hpp"

#include "audio.hpp"
#include "baseband_api.hpp"
#include "dcs.hpp"
#include "file_path.hpp"
#include "memory_map.hpp"
#include "string_format.hpp"

#include <cstdlib>

using namespace portapack;

namespace ui::external_app::table_scan_rx {

namespace {

constexpr uint32_t SCAN_DWELL_FRAMES = 15;

uint32_t parse_frequency(std::string_view value) {
    const std::string text{value};
    return static_cast<uint32_t>(std::atof(text.c_str()) * 1'000'000.0 + 0.5);
}

uint32_t parse_tone(std::string_view value) {
    const std::string text{value};
    return static_cast<uint32_t>(std::atof(text.c_str()) * 10.0 + 0.5);
}

uint16_t parse_dtcs_code(std::string_view value) {
    uint16_t code = 0;
    for (const char digit : value) {
        if (digit < '0' || digit > '7') return 0;
        code = static_cast<uint16_t>(code * 8 + digit - '0');
    }
    return code <= 511 ? code : 0;
}

std::string frequency_text(rf::Frequency frequency) {
    return to_string_dec_uint(frequency / 1'000'000) + "." +
           to_string_dec_uint(frequency % 1'000'000, 6, '0');
}

}  // namespace

TableScanRxView::TableScanRxView(NavigationView& nav)
    : nav_{nav} {
    add_children({&rssi, &field_volume, &text_file, &text_channel, &text_status,
                  &button_load, &button_scan, &field_squelch, &field_channel});

    field_squelch.set_value(static_cast<int32_t>(squelch_));
    field_squelch.on_change = [this](int32_t value) {
        squelch_ = static_cast<uint32_t>(value);
        if (running_) tune_channel(channel_);
    };

    field_channel.on_change = [this](size_t, int32_t value) {
        if (value >= 0 && static_cast<size_t>(value) < channels_.size()) {
            channel_ = static_cast<uint32_t>(value);
            detected_ = false;
            dwell_frames_ = 0;
            tune_channel(channel_);
            update_status();
        }
    };

    button_load.on_select = [this](Button&) {
        auto open_view = nav_.push<FileLoadView>(".TXT");
        open_view->push_dir(freqman_dir);
        open_view->on_changed = [this](std::filesystem::path path) {
            load_table(path);
        };
    };

    button_scan.on_select = [this](Button&) {
        if (scanning_)
            stop_scan();
        else
            start_scan();
    };

    load_table(freqman_dir / table_filename);
}

TableScanRxView::~TableScanRxView() {
    stop_receiver();
}

void TableScanRxView::focus() {
    button_load.focus();
}

void TableScanRxView::load_table(const std::filesystem::path& path) {
    File file;
    if (file.open(path)) {
        text_status.set("Add FREQMAN/CHIRP.TXT");
        return;
    }

    if (path.filename().string() != table_filename) {
        text_status.set("Use FREQMAN/CHIRP.TXT");
        return;
    }

    std::vector<Channel> loaded;
    FileLineReader reader(file);
    bool header = true;
    for (const auto& line : reader) {
        if (header) {
            header = false;
            continue;
        }

        auto columns = split_string(line, '\t');
        if (columns.size() < 10) continue;

        const uint32_t frequency = parse_frequency(columns[2]);
        if (frequency == 0) continue;

        Channel channel{};
        channel.frequency = frequency;
        channel.name = columns[1].empty() ? ("Channel " + to_string_dec_uint(loaded.size() + 1)) : std::string{columns[1]};
        if (columns[5] == "TSQL") {
            channel.ctcss_x10 = parse_tone(columns[7]);
        } else if (columns[5] == "DTCS") {
            channel.dtcs_code = parse_dtcs_code(columns[8]);
            channel.dtcs_reverse = !columns[9].empty() && columns[9][0] == 'R';
        } else {
            continue;
        }
        loaded.push_back(std::move(channel));
    }

    if (loaded.empty()) {
        text_status.set("No TSQL/DTCS channels found");
        return;
    }

    channels_ = std::move(loaded);
    channel_ = 0;
    update_channel_options();
    text_file.set("Table: CHIRP.TXT");
    tune_channel(channel_);
    update_status();
}

void TableScanRxView::update_channel_options() {
    std::vector<std::pair<std::string, int32_t>> options;
    options.reserve(channels_.size());
    for (size_t i = 0; i < channels_.size(); i++) {
        options.emplace_back(
            to_string_dec_uint(i + 1) + " " + frequency_text(channels_[i].frequency),
            static_cast<int32_t>(i));
    }
    field_channel.set_options(options);
    field_channel.set_by_value(static_cast<int32_t>(channel_));
}

void TableScanRxView::tune_channel(uint32_t index) {
    if (index >= channels_.size()) return;
    const auto& channel = channels_[index];
    receiver_model.set_target_frequency(channel.frequency);
    if (running_) {
        baseband::set_tonedetect_config(
            static_cast<uint8_t>(squelch_),
            channel.ctcss_x10,
            channel.dtcs_code > 0 ? dcs::dcs_word(channel.dtcs_code) : 0,
            channel.dtcs_reverse);
    }
}

void TableScanRxView::start_receiver() {
    if (running_) return;
    baseband::run_prepared_image(portapack::memory::map::m4_code.base());
    audio::set_rate(audio::Rate::Hz_24000);
    audio::output::start();
    receiver_model.set_sampling_rate(3072000);
    receiver_model.set_baseband_bandwidth(1750000);
    receiver_model.enable();
    running_ = true;
    tune_channel(channel_);
}

void TableScanRxView::stop_receiver() {
    if (!running_) return;
    scanning_ = false;
    receiver_model.disable();
    baseband::shutdown();
    audio::output::stop();
    running_ = false;
}

void TableScanRxView::start_scan() {
    if (channels_.empty()) {
        text_status.set("Load a table first");
        return;
    }
    start_receiver();
    detected_ = false;
    dwell_frames_ = 0;
    scanning_ = true;
    button_scan.set_text("Stop");
    tune_channel(channel_);
    update_status();
}

void TableScanRxView::stop_scan() {
    scanning_ = false;
    button_scan.set_text("Scan");
    update_status();
}

void TableScanRxView::advance_channel() {
    if (channels_.empty()) return;
    channel_ = (channel_ + 1) % channels_.size();
    field_channel.set_by_value(static_cast<int32_t>(channel_));
    dwell_frames_ = 0;
    tune_channel(channel_);
    update_status();
}

void TableScanRxView::on_framesync() {
    if (!scanning_ || detected_) return;
    if (++dwell_frames_ >= SCAN_DWELL_FRAMES)
        advance_channel();
}

void TableScanRxView::on_tone_data(const ToneDetectDataMessage* message) {
    if (!scanning_ || message->tone_end || message->freq_hz == 0) return;
    detected_ = true;
    scanning_ = false;
    button_scan.set_text("Scan");
    text_status.set("Found signal on channel " + to_string_dec_uint(channel_ + 1));
}

void TableScanRxView::update_status() {
    if (channels_.empty()) {
        text_channel.set("No channels loaded");
        return;
    }
    const auto& channel = channels_[channel_];
    const std::string mode = channel.dtcs_code > 0
                                 ? "DTCS " + to_string_dec_uint(channel.dtcs_code) + (channel.dtcs_reverse ? "R" : "N")
                                 : "TSQL " + to_string_dec_uint(channel.ctcss_x10 / 10) + "." + to_string_dec_uint(channel.ctcss_x10 % 10) + "Hz";
    text_channel.set("CH " + to_string_dec_uint(channel_ + 1) + "  " + frequency_text(channel.frequency) + "  " + mode);
    if (!detected_)
        text_status.set(scanning_ ? "Scanning..." : "Ready");
}

}  // namespace ui::external_app::table_scan_rx
