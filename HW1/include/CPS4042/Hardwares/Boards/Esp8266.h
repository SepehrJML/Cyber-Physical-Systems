#ifndef ESP8266_H
#define ESP8266_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>
#include <iostream>

namespace Boards
{

using Esp8266Voltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct Esp8266Gpio
{
public:
    Pins::Vdd<WorkingVoltageTp> vdd1 {BR, BTR, "Esp8266::vdd1"};
    Pins::Gnd<WorkingVoltageTp> gnd1 {BR, BTR, "Esp8266::gnd1"};
    Pins::Vdd<WorkingVoltageTp> vdd2 {BR, BTR, "Esp8266::vdd2"};
    Pins::Gnd<WorkingVoltageTp> gnd2 {BR, BTR, "Esp8266::gnd2"};
    Pins::Vdd<WorkingVoltageTp> vdd3 {BR, BTR, "Esp8266::vdd3"};
    Pins::Gnd<WorkingVoltageTp> gnd3 {BR, BTR, "Esp8266::gnd3"};
    Pins::Rx<WorkingVoltageTp> rx {BR, BTR, "Esp8266::rx"};
    Pins::Tx<WorkingVoltageTp> tx {BR, BTR, "Esp8266::tx"};
    Pins::Sda<WorkingVoltageTp> sda {BR, BTR, "Esp8266::sda"};
    Pins::Scl<WorkingVoltageTp> scl {BR, BTR, "Esp8266::scl"};
    Pins::Digital<WorkingVoltageTp> d0 {BR, BTR, "Esp8266::d0"};
    Pins::Digital<WorkingVoltageTp> d1 {BR, BTR, "Esp8266::d1"};
    Pins::Digital<WorkingVoltageTp> d2 {BR, BTR, "Esp8266::d2"};
    Pins::Digital<WorkingVoltageTp> d3 {BR, BTR, "Esp8266::d3"};
    Pins::Digital<WorkingVoltageTp> d4 {BR, BTR, "Esp8266::d4"};
    Pins::Digital<WorkingVoltageTp> d5 {BR, BTR, "Esp8266::d5"};
    Pins::Analog<WorkingVoltageTp> a0 {BR, BTR, "Esp8266::a1"};
};

class Esp8266 : public Board<BaudRates::B115200, BitRates::same(BaudRates::B115200), Frequency::F320khz, Esp8266Voltage, Esp8266Gpio>
{
public:
    explicit Esp8266() : Parent {"Esp8266::Processor"}
    {
        m_processor->communicationClockChanged.connect(
            [this](Bit edge) { m_gpio.scl.nextEdge(edge); });

        m_processor->installProtocol(&i2c);
        m_processor->installProtocol(&usart);
        std::cout << "one instance of Esp8266 created." << std::endl;
    };

    class I2C : public Protocols::AbstractI2C<Esp8266, Gpio>
    {
    public:
        explicit I2C(Esp8266* b) : Protocols::AbstractI2C<Esp8266, Gpio> {b} {}

        void init(Byte address) override
        {
            m_targetAddress = address;
            this->m_started = true;
            m_state = 0;
            while(!this->m_buffer.empty()) this->m_buffer.pop();
        }

        void write(Byte byte) override { this->m_buffer.push(byte); }

        Byte read() override
        {
            if(this->m_buffer.empty()) return 0;
            Byte b = this->m_buffer.front();
            this->m_buffer.pop();
            return b;
        }

        void run(Gpio& gpio) override
        {
            switch (m_state) {
                case 0: 
                    if (this->m_started) {
                        gpio.sda.setCanRead(false);
                        while(gpio.sda.hasBitToRead()) gpio.sda.readBit(); 
                        gpio.sda.write(m_targetAddress);
                        this->m_started = false;
                        m_state = 1;
                        m_rxEnabled = false;
                    }
                    break;
                    
                case 1: 
                    if (gpio.sda.hasBitToWrite()) return;
                    
                    if (!m_rxEnabled) {
                        gpio.sda.setCanRead(true);
                        m_rxEnabled = true;
                        return;
                    }
                    
                    while (gpio.sda.hasBitToRead()) {
                        Bit ack = gpio.sda.readBit();
                        if (ack == Bit::One) {
                            m_state = 2;
                            m_bytesRead = 0;
                            break;
                        }
                    }
                    break;
                    
                case 2:
                    while (gpio.sda.hasByteToRead()) {
                        this->m_buffer.push(gpio.sda.read());
                        m_bytesRead++;
                        if (m_bytesRead >= 3) {
                            gpio.sda.setCanRead(false);
                            while(gpio.sda.hasBitToRead()) gpio.sda.readBit();
                            m_state = 3;
                            break;
                        }
                    }
                    break;
                    
                case 3:
                    gpio.sda.setCanRead(false);
                    break;
            }
        }
        
    private:
        int m_state = 3;
        int m_bytesRead = 0;
        bool m_rxEnabled = false;
        Byte m_targetAddress = 0;
    } mutable i2c {this};

    class USART : public Protocols::AbstractUsart<Esp8266, Gpio>
    {
    public:
        explicit USART(Esp8266* b) : Protocols::AbstractUsart<Esp8266, Gpio> {b} {}
        
        void write(Byte byte) override { this->m_buffer.push(byte); }

        Byte read() override {
            if(this->m_buffer.empty())
                return 0;

            Byte b = this->m_buffer.front();
            this->m_buffer.pop();

            return b;
        }

        void run(Gpio& gpio) override {
            gpio.tx.setCanRead(false);
            gpio.rx.setCanRead(true);

            if(!m_isWritingFinished && !this->m_buffer.empty())
            {
                if(!gpio.tx.hasBitToWrite())
                {
                    Byte b = this->m_buffer.front();
                    this->m_buffer.pop();

                    std::cout << "ESP8266 TX: " << std::hex << static_cast<int>(b) << std::endl;

                    gpio.tx.write(b);

                    m_isWritingFinished = true;
                }
            }

            if(m_isWritingFinished && !gpio.tx.hasBitToWrite())
            {
                m_isWritingFinished = false;
            }

            while(gpio.rx.hasByteToRead())
            {
                Byte received = gpio.rx.read();

                std::cout << "ESP RX: 0x"
                        << std::hex
                        << (static_cast<int>(received) & 0xFF)
                        << std::endl;
            }
        }

    } mutable usart {this};

protected:
    inline void startModule() override {}
};

} // namespace Boards

#endif // ESP8266_H