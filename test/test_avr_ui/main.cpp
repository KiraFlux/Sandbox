#include <Arduino.h>
#include <kf/UI.hpp>
#include <kf/ui/TextRender.hpp>

using TUI = kf::UI<kf::ui::TextRender>;

// Объявляем функции вместо лямбд в глобальной области
void button1Click() { Serial.println(F("Button 1 click")); }

struct SimplePage : TUI::Page {

    TUI::Button btn1{
        *this,
        "Button 1",
        []() {
            button1Click();
        }};

    TUI::Button btn2;

    SimplePage() :
        Page{"Simple"},
        btn2{*this, "Button 2", []() {
                 Serial.println(F("Button 2 click")); // F: # define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
             }} {}
};

SimplePage page1{};

static auto &tui = TUI::instance();

void setup() {
    static char text_buffer[128];

    Serial.begin(115200);

    auto &rs = tui.getRenderSettings();
    rs.buffer = {reinterpret_cast<kf::u8 *>(text_buffer), sizeof(text_buffer)};
    rs.rows_total = 10;
    rs.row_max_length = 28;
    rs.on_render_finish = [](const kf::slice<const kf::u8> &str) {
        Serial.write(str.data(), str.size());
        Serial.println();
    };

    tui.bindPage(page1);
    tui.addEvent(TUI::Event::Update());
}

void loop() {

    tui.poll();
}