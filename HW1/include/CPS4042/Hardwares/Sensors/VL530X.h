#ifndef VL53_X_H
#define VL53_X_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>
#include <iostream>

namespace Sensors
{
using Vl530xVoltage = VoltageLevel3_3v;

template <std::uint64_t bar, std::uint64_t btr, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct Vl530xGpio
{
public:
    Pins::Vdd<WorkingVoltageTp> vdd {bar, btr, "Vl530x::vdd"};
    Pins::Gnd<WorkingVoltageTp> gnd {bar, btr, "Vl530x::gnd"};
    Pins::Sda<WorkingVoltageTp> sda {bar, btr, "Vl530x::sda"};
    Pins::Scl<WorkingVoltageTp> scl {bar, btr, "Vl530x::scl"};
};

class Vl530x : public Board<BaudRates::NotSpecified, BitRates::same(BaudRates::NotSpecified), Frequency::Drived, Vl530xVoltage, Vl530xGpio>
{
public:
    inline static constexpr Byte address = 0x29;
    enum class Mode
    {
        I2C,
        MUX
    };
        
    explicit Vl530x(Mode m = Mode::I2C)
        : Parent {"Vl530x::processor"},
        m_mode(m)
    {
        m_processor->installProtocol(&i2c);
        std::cout << "one instance of Vl530x created." << std::endl;
    }

    class I2C : public Protocols::AbstractI2C<Vl530x, Gpio>
    {
    public:
        explicit I2C(Vl530x* b) : Protocols::AbstractI2C<Vl530x, Gpio> {b} {}

        void init(Byte address) override
        {
            this->m_started = true;
            std::cout << "[SENSOR] init called" << std::endl;
        }

        void write(Byte byte) override
        {
            this->m_buffer.push(byte);
            std::cout << "[SENSOR] buffered byte 0x"
                      << std::hex
                      << (static_cast<unsigned int>(byte) & 0xFF)
                      << std::dec
                      << std::endl;
        }

        Byte read() override
        {
            if(this->m_buffer.empty())
                return 0;

            Byte b = this->m_buffer.front();
            this->m_buffer.pop();
            return b;
        }

        void run(Gpio& gpio) override
        {
            static int sensorDivider = 0;
            ++sensorDivider;

            switch(m_state)
            {
                case 0:
                {
                    gpio.sda.setCanRead(true);
                    if(gpio.sda.hasByteToRead())
                    {
                        Byte addr = gpio.sda.read();
                        if (addr == (Vl530x::address) )
                        {
                            gpio.sda.setCanRead(false);
                            m_state = 1;
                            m_waitCycle = 0;
                            std::cout << "[SENSOR] transition S0 -> S1" << std::endl;
                        }
                        else
                        {
                            while(gpio.sda.hasBitToRead())
                            {
                                Bit dumped = gpio.sda.readBit();
                            }
                        }
                    }

                    break;
                }

                case 1:
                {
                    if(this->m_board->m_mode == Mode::I2C) {
                    if(m_waitCycle < 2)
                    {
                        m_waitCycle++;
                        return;
                    }
                    } else {
                        if(m_waitCycle < 20)
                        {
                        m_waitCycle++;
                        return;
                        }
                    }

                    std::cout << "[SENSOR][S1] sending ACK bit=1" << std::endl;
                    gpio.sda.write(Bit::One);
                    m_state = 2;
                    std::cout << "[SENSOR] transition S1 -> S2" << std::endl;
                    break;
                }

                case 2:
                {
                    
                    if(this->m_board->m_mode == Mode::I2C) {
                        if(gpio.sda.hasBitToWrite())
                            return;

                        while(!this->m_buffer.empty())
                        {
                            Byte out = this->m_buffer.front();

                            std::cout << "[SENSOR][S2] sending byte 0x"
                                    << std::hex
                                    << (unsigned)out
                                    << std::dec
                                    << std::endl;

                            gpio.sda.write(out);
                            this->m_buffer.pop();
                        }

                        m_state = 3;
                        std::cout << "[SENSOR] transition S2 -> S3" << std::endl;
                        break;
                    }else {
                        if(gpio.sda.hasBitToWrite())
                            return;

                        if(!this->m_buffer.empty())
                        {
                            Byte out = this->m_buffer.front();

                            std::cout << "[SENSOR][S2] sending byte 0x"
                                    << std::hex
                                    << (static_cast<unsigned int>(out) & 0xFF)
                                    << std::dec
                                    << std::endl;

                            gpio.sda.write(out);
                            this->m_buffer.pop();

                            m_idleCycles = 0;
                            m_state = 3;

                            std::cout << "[SENSOR] transition S2 -> S3" << std::endl;
                        }    
                    }
                    break;
                }

                case 3:
                    if (gpio.sda.hasBitToWrite()) return;
                    gpio.sda.setCanRead(true);
                    while(gpio.sda.hasBitToRead()) gpio.sda.readBit(); 
                    m_state = 0; 
                    break;
                default:
                {
                    if(sensorDivider % 20 == 0)
                    {
                        std::cout << "[SENSOR][S3] idleCycles="
                                << m_idleCycles
                                << std::endl;
                    }

                    gpio.sda.setCanRead(false);

                    if(++m_idleCycles >= 2)
                    {
                        m_idleCycles = 0;
                        m_state = 0;

                        std::cout << "[SENSOR] bus idle, transition S3 -> S0"
                                << std::endl;
                    }

                    break;
                }
            }
        }

    private:
        int m_state = 0;
        int m_waitCycle = 0;
        int m_idleCycles = 0;
    } mutable i2c {this};

protected:
    inline void startModule() override
    {
        m_gpio.scl.onNextEdge([this](Vl530xVoltage level) {
            auto bit = Voltage::toBit(level);
            if(bit == Bit::One) {
                m_processor->nextCycle(m_gpio);
            }
        });
    }
private: 
    Mode m_mode;
};

} // namespace Sensors

#endif // VL53_X_H