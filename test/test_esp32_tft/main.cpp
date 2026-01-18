#include <Arduino.h>
#include <SPI.h>

// color masks
#define RED 0b111111'00000'00000
#define BLUE 0b000000'11111'00000
#define GREEN 0b000000'00000'11111

using u8 = uint8_t;
using u32 = uint32_t;

struct ST7735 {

    using PixelValue = uint16_t;

    enum class Orientation : u8 {
        // todo Битовые поля задают что менять (w, h) -- (h, w)
        Normal = 0x00,
        ClockWise = 0x01,
        Flipped = 0x02,
        CounterClockWise = 0x03,
    };

    struct Settings {
        Orientation orientation;
        u8 pin_spi_slave_select;
        u8 pin_data_command;
        u8 pin_reset;
        u32 spi_frequency;

        constexpr explicit Settings(
            gpio_num_t spi_cs,
            gpio_num_t dc,
            gpio_num_t reset,
            u32 spi_freq = 27000000u,
            Orientation orientation = Orientation::Normal) :
            pin_spi_slave_select{static_cast<u8>(spi_cs)},
            pin_data_command{static_cast<u8>(dc)},
            pin_reset{static_cast<u8>(reset)},
            spi_frequency{spi_freq},
            orientation{orientation} {}
    };

private:
    static constexpr auto phys_width{128};
    static constexpr auto phys_height{160};
    static constexpr auto buffer_length{phys_width * phys_height};

    const Settings &settings;
    SPIClass &spi;

    PixelValue buffer[buffer_length]{};
    u8 logical_width{phys_width};
    u8 logical_height{phys_height};

public:
    explicit ST7735(const Settings &settings, SPIClass &spi_instance) :
        settings{settings}, spi{spi_instance} {}

    [[nodiscard]] inline u8 width() const { return logical_width; }

    [[nodiscard]] inline u8 height() const { return logical_height; }

    [[nodiscard]] inline PixelValue *bufferData() { return buffer; }

    [[nodiscard]] inline constexpr size_t bufferLength() const { return buffer_length; }// NOLINT(*-convert-member-functions-to-static)

    [[nodiscard]] bool init() {
        pinMode(settings.pin_spi_slave_select, OUTPUT);
        pinMode(settings.pin_data_command, OUTPUT);
        pinMode(settings.pin_reset, OUTPUT);

        spi.begin();
        spi.setFrequency(settings.spi_frequency);

        digitalWrite(settings.pin_reset, LOW);
        delay(10);
        digitalWrite(settings.pin_reset, HIGH);
        delay(120);

        sendCommand(Command::SWRESET);
        delay(150);

        sendCommand(Command::SLPOUT);
        delay(255);

        sendCommand(Command::COLMOD);
        const u8 color_mode{0x05};// 16-bit color
        sendData(&color_mode, 1);

        setOrientation(settings.orientation);

        sendCommand(Command::DISPON);
        delay(100);

        return true;
    }

    // todo Частичное обновление буфера через систему грязных областей (битовая матрица изменений меньшего размера)
    void send() {
        setWindow(0, 0, logical_width - 1, logical_height - 1);

        sendCommand(Command::RAMWR);
        sendData(reinterpret_cast<const u8 *>(buffer), sizeof(buffer));
    }

    void setOrientation(Orientation orientation) {
        u8 mad_ctl = MADCTL_BGR;

        switch (orientation) {
            case Orientation::Normal:
                logical_width = phys_width;
                logical_height = phys_height;
                break;

            case Orientation::ClockWise:
                mad_ctl |= MADCTL_MX;
                mad_ctl |= MADCTL_MV;
                logical_width = phys_height;
                logical_height = phys_width;
                break;

            case Orientation::Flipped:
                mad_ctl |= MADCTL_MX;
                mad_ctl |= MADCTL_MY;
                logical_width = phys_width;
                logical_height = phys_height;
                break;

            case Orientation::CounterClockWise:
                mad_ctl |= MADCTL_MY;
                mad_ctl |= MADCTL_MV;
                logical_width = phys_height;
                logical_height = phys_width;
                break;
        }

        sendCommand(Command::MADCTL);
        sendData(&mad_ctl, sizeof(mad_ctl));
    }

private:
    enum MadCtl : u8 {
        MADCTL_RGB = 0x00,

        /// Меняет местами строки и столбцы
        MADCTL_MV = 0x20,

        /// Зеркальное отображение по горизонтали
        MADCTL_MX = 0x40,

        /// Зеркальное отображение по вертикали
        MADCTL_MY = 0x80,

        MADCTL_BGR = 0x08,
    };

    void setWindow(u8 x0, u8 y0, u8 x1, u8 y1) const {
        const u8 x[] = {0x00, x0, 0x00, x1};
        sendCommand(Command::CASET);
        sendData(x, sizeof(x));

        sendCommand(Command::RASET);
        const u8 y[] = {0x00, y0, 0x00, y1};
        sendData(y, sizeof(y));
    }

    void sendData(const u8 *data, size_t size) const {
        digitalWrite(settings.pin_data_command, HIGH);
        digitalWrite(settings.pin_spi_slave_select, LOW);
        spi.writeBytes(data, size);
        digitalWrite(settings.pin_spi_slave_select, HIGH);
    }

    enum class Command : u8 {
        SWRESET = 0x01,

        SLPIN = 0x10,
        SLPOUT = 0x11,

        INVOFF = 0x20,
        INVON = 0x21,
        DISPOFF = 0x28,
        DISPON = 0x29,
        CASET = 0x2A,
        RASET = 0x2B,
        RAMWR = 0x2C,

        MADCTL = 0x36,
        COLMOD = 0x3A
    };

    void sendCommand(Command command) const {
        digitalWrite(settings.pin_data_command, LOW);
        digitalWrite(settings.pin_spi_slave_select, LOW);
        spi.write(static_cast<u8>(command));
        digitalWrite(settings.pin_spi_slave_select, HIGH);
    }
};

static ST7735::Settings display_settings{
    GPIO_NUM_5,
    GPIO_NUM_2,
    GPIO_NUM_15,
    27000000,
};

static ST7735 &getDisplay() {
    static ST7735 st_7735{
        display_settings,
        SPI,
    };
    return st_7735;
}

static auto &display = getDisplay();

static void setPixel(u8 x, u8 y, uint16_t color) {
    if (x < display.width() and y < display.height()) {
        display.bufferData()[y * display.width() + x] = color;
    }
}

static void fill(uint16_t color) {
    for (auto p = display.bufferData(), end = display.bufferData() + display.bufferLength(); p < end; p += 1) {
        *p = color;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting display init...");

    (void) display.init();
    Serial.println("Display initialized");

    for (u8 i = 0; i < 4; i += 1) {
        Serial.printf("Testing rotation %d...\n", i);
        display.setOrientation(static_cast<ST7735::Orientation>(i & 0b11));

        fill(0x0000);

        for (u8 x = 0; x < 20; ++x) {
            for (u8 y = 0; y < 60; ++y) {
                setPixel(x, y, RED);
                setPixel(display.width() - x, y, BLUE);
                setPixel(display.width() - x, display.height() - y, GREEN);
            }
        }

        display.send();
        delay(2000);
    }

    Serial.println("Test complete");
}

void loop() {}