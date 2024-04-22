#include "app_chat.h"
#include "lgfx/v1/misc/enum.hpp"
#include "spdlog/spdlog.h"
#include <cstdint>
#include "../utils/theme/theme_define.h"
#include "../utils/esp_now_wrap/esp_now_wrap.h"

using namespace MOONCAKE::APPS;

#define _keyboard _data.hal->keyboard()
#define _canvas _data.hal->canvas()
#define _canvas_update _data.hal->canvas_update
#define _canvas_clear() _canvas->fillScreen(THEME_COLOR_BG)
#define MESSAGE_AREA_HEIGHT (_canvas->height() - 25) // Space for the input box

void AppChat::_update_input()
{
    if (_keyboard->keyList().size() != _data.last_key_num)
    {
        _keyboard->updateKeysState();
        if (_keyboard->keysState().enter && !_data.repl_input_buffer.empty())
        {
            espnow_wrap_send((uint8_t *)_data.repl_input_buffer.c_str(), _data.repl_input_buffer.size());
            _draw_message(_data.repl_input_buffer, true);
            _data.repl_input_buffer = "";
            _data.hal->Speaker()->tone(1500, 200);
            delay(200);
            _data.hal->Speaker()->tone(1000, 200);
        }
        else
        {
            if (_keyboard->keysState().space)
                _data.repl_input_buffer += ' ';
            else if (_keyboard->keysState().del && !_data.repl_input_buffer.empty())
                _data.repl_input_buffer.pop_back();
            else
                _append_input_chars();
            _update_input_panel();
        }
        _data.last_key_num = _keyboard->keyList().size();
    }
}

void AppChat::_update_cursor()
{
    if ((millis() - _data.cursor_update_time_count) > _data.cursor_update_period)
    {
        _data.cursor_state = !_data.cursor_state;
        _data.cursor_update_time_count = millis();
    }
}

void AppChat::_update_input_panel()
{
    _update_cursor();

    // Fill the bottom area with white color for the input panel
    _canvas->fillRect(0, MESSAGE_AREA_HEIGHT, _canvas->width(), 25, THEME_COLOR_REPL_TEXT);

    // Draw separation line above the input panel
    _canvas->drawFastHLine(0, MESSAGE_AREA_HEIGHT - 1, _canvas->width(), THEME_COLOR_BG);

    // Prepare the input text with cursor
    snprintf(_data.string_buffer, sizeof(_data.string_buffer), ">> %s%c", _data.repl_input_buffer.c_str(), _data.cursor_state ? '_' : ' ');

    // Set text color to black for high contrast on the white bar
    _canvas->setTextColor(THEME_COLOR_BG, THEME_COLOR_REPL_TEXT);

    // Draw the input text on the white bar
    _canvas->drawString(_data.string_buffer, 5, MESSAGE_AREA_HEIGHT);

    // Update the canvas to reflect these changes
    _canvas_update();

    // Reset the text color if needed elsewhere
    _canvas->setTextColor(THEME_COLOR_REPL_TEXT, THEME_COLOR_BG);
}

void AppChat::_update_receive()
{
    if (espnow_wrap_available())
    {
        _data.hal->Speaker()->tone(1000, 200);
        delay(200);
        _data.hal->Speaker()->tone(1500, 200);
        const char *msg = (char *)espnow_wrap_get_received();
        _draw_message(msg, false);
        espnow_wrap_clear();
    }
}

void AppChat::_draw_message(const std::string &message, bool isSender)
{
    // Add new message and maintain only the last three messages
    last_messages.push_back(std::make_pair(message, isSender));
    if (last_messages.size() > 3)
    {
        last_messages.pop_front();
    }

    _redraw_messages();
}

void AppChat::_redraw_messages()
{
    _canvas_clear();
    _update_input_panel(); // Ensure the input panel is redrawn after clearing

    int y_offset = 0; // Start from the top after clearing the canvas

    // Iterate through the deque and draw each message
    for (const auto &msg : last_messages)
    {
        if (msg.second)
        { // Check if the message is from the sender
            int text_width = _canvas->textWidth(msg.first.c_str());
            _canvas->drawString(msg.first.c_str(), _canvas->width() - text_width - 10, y_offset);
        }
        else
        {
            _canvas->drawString(msg.first.c_str(), 10, y_offset);
        }
        y_offset += _canvas->fontHeight() + 5;
    }
}

void AppChat::_append_input_chars()
{
    for (char c : _keyboard->keysState().values)
        _data.repl_input_buffer += c;
}

void AppChat::onCreate()
{
    spdlog::info("{} onCreate", getAppName());
    _data.hal = mcAppGetDatabase()->Get("HAL")->value<HAL::Hal *>();

    // Initialize last_messages with two empty messages
    last_messages.push_back(std::make_pair("", false));
    last_messages.push_back(std::make_pair("", false));

    // Initialize sound
    _data.hal->Speaker()->end();

    { /// custom setting
        auto spk_cfg = _data.hal->Speaker()->config();
        /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
        // spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
        spk_cfg.task_pinned_core = PRO_CPU_NUM;
        _data.hal->Speaker()->config(spk_cfg);
    }

    _data.hal->Speaker()->begin();
}

void AppChat::onResume()
{
    spdlog::info("{} onResume", getAppName());
    _canvas_clear();
    _canvas->setTextColor(THEME_COLOR_REPL_TEXT, THEME_COLOR_BG);
    _canvas->setFont(&fonts::efontCN_24_b);
    _canvas->setTextSize(FONT_SIZE_REPL);
    _canvas->print("\n\n");
    _update_input_panel();
    espnow_wrap_init();
}

void AppChat::onRunning()
{
    _update_input_panel();
    _update_input();
    _update_receive();
    if (_data.hal->homeButton()->pressed())
    {
        _data.hal->playNextSound();
        spdlog::info("quit chat");
        destroyApp();
    }
}
