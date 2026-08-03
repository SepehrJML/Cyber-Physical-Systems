#ifndef MICROCONTROLLER_H
#define MICROCONTROLLER_H

#include <CPS4042/Hardwares/Boards/Esp8266.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <CPS4042/Utils/ByteStream.h>

#include <iostream>
#include <algorithm>
#include <chrono>

class MicroController : public AbstractSketch<Boards::Esp8266>
{
public:
    enum class Mode
    {
        Mux,
        I2C,
        USART
    };

    explicit MicroController(
        Boards::Esp8266* node,
        Mode mode = Mode::Mux
    )
        : AbstractSketch<Boards::Esp8266>{node},
          m_mode(mode)
    {}

    std::int32_t setup(Boards::Esp8266::Gpio& gpio) override
    {
        std::cout << "esp8266 setup completed." << std::endl;

        if(m_mode == Mode::Mux)
        {
            node()->usart.write(static_cast<Byte>(0x00));
        }

        return 0;
    }

    std::int32_t loop(Boards::Esp8266::Gpio& gpio) override
    {
        if(m_mode == Mode::Mux)
            return muxLoop(gpio);
        if (m_mode == Mode::USART)
            return usartLoop(gpio);
        return i2cLoop(gpio);
    }

private:

    /*
    =========================
        USART MODE
    =========================
    */

    std::int32_t usartLoop(Boards::Esp8266::Gpio&)
    {
        static Byte currentChannel = 0x00;

        static auto lastSwitch =
            std::chrono::steady_clock::now();

        auto now = std::chrono::steady_clock::now();

        if(std::chrono::duration_cast<
            std::chrono::milliseconds>(
                now - lastSwitch).count() >= 1000)
        {
            

            node()->usart.write(currentChannel);
            currentChannel++;
            lastSwitch = now;
        }
        return 0;
    }

    /*
    =========================
        MUX MODE
    =========================
    */

    std::int32_t muxLoop(Boards::Esp8266::Gpio&)
    {
        static Byte currentChannel = 0;

        static auto lastSwitch =
            std::chrono::steady_clock::now();

        auto now = std::chrono::steady_clock::now();

        if(std::chrono::duration_cast<
            std::chrono::milliseconds>(
                now - lastSwitch).count() >= 1000)
        {
            currentChannel ^= 0x01;

            std::cout
                << "[ESP] switching to channel "
                << (int)(currentChannel & 0xFF)
                << std::endl;

            node()->usart.write(currentChannel);

            lastSwitch = now;
        }

        return 0;
    }

    /*
    =========================
        DIRECT I2C MODE
    =========================
    */

    std::int32_t i2cLoop(Boards::Esp8266::Gpio& gpio)
    {
        static ByteStream<std::uint16_t> distanceStream;

        static Byte b1 = 0;
        static Byte b2 = 0;
        static int state = 0;

        static auto lastTime =
            std::chrono::steady_clock::now();

        auto now = std::chrono::steady_clock::now();

        if(std::chrono::duration_cast<
            std::chrono::milliseconds>(
                now - lastTime).count() >= 1000)
        {
            gpio.sda.setCanRead(false);

            while(gpio.sda.hasBitToRead())
                gpio.sda.readBit();

            while(node()->i2c.isDataAvailable())
                node()->i2c.read();

            distanceStream.clear();

            node()->i2c.init(0x29);

            state = 0;
            lastTime = now;
        }

        while(node()->i2c.isDataAvailable())
        {
            Byte b = node()->i2c.read();

            if(state == 0)
            {
                b1 = b;
                state = 1;
            }
            else if(state == 1)
            {
                b2 = b;
                state = 2;
            }
            else
            {
                Byte checksum = b;

                UByte ub1 =
                    static_cast<UByte>(b1);

                UByte ub2 =
                    static_cast<UByte>(b2);

                Byte expected =
                    static_cast<Byte>(
                        std::max(ub1, ub2)
                        -
                        std::min(ub1, ub2)
                    );

                if(checksum == expected)
                {
                    distanceStream.clear();

                    distanceStream << b1;
                    distanceStream << b2;

                    if(distanceStream.isReady())
                    {
                        std::cout
                            << "new distance: "
                            << distanceStream.take()
                            << " mm\n";
                    }
                }
                else
                {
                    std::cerr
                        << "Invalid checksum received\n";
                }

                state = 0;
            }
        }

        return 0;
    }

private:
    Mode m_mode;
};

#endif // MICROCONTROLLER_H