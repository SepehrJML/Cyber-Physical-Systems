#ifndef I2CMUX_H
#define I2CMUX_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>

#include <iostream>
#include <queue>
#include <type_traits>

namespace Sensors
{

using I2CMuxVoltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct I2CMuxGpio
{
public:
    Pins::Vdd<WorkingVoltageTp> vdd {BR, BTR, "I2CMux::vdd"};
    Pins::Gnd<WorkingVoltageTp> gnd {BR, BTR, "I2CMux::gnd"};

    Pins::Rx<WorkingVoltageTp> rx {BR, BTR, "I2CMux::rx"};
    Pins::Tx<WorkingVoltageTp> tx {BR, BTR, "I2CMux::tx"};

    Pins::Sda<WorkingVoltageTp> sda0 {BR,BTR,"I2CMux::sda0"};
    Pins::Scl<WorkingVoltageTp> scl0 {BR,BTR,"I2CMux::scl0"};

    Pins::Sda<WorkingVoltageTp> sda1 {BR,BTR,"I2CMux::sda1"};
    Pins::Scl<WorkingVoltageTp> scl1 {BR,BTR,"I2CMux::scl1"};
};

class I2CMux : public Board<
    BaudRates::B115200,
    BitRates::same(BaudRates::B115200),
    Frequency::F320khz,
    I2CMuxVoltage,
    I2CMuxGpio>
{
public:
    explicit I2CMux()
    : Parent {"I2CMux::processor"}
    {
        m_processor->communicationClockChanged.connect(
            [this](Bit edge)
            {
                m_gpio.scl0.nextEdge(edge);
                m_gpio.scl1.nextEdge(edge);
            });

        m_processor->installProtocol(&i2c0);
        m_processor->installProtocol(&i2c1);
        m_processor->installProtocol(&usart);

        //i2c.setChannel(0);
        //i2c.init(0x29);

        std::cout << "one instance of I2CMux created." << std::endl;
    }

    class USART : public Protocols::AbstractUsart<I2CMux, Gpio>
    {
    public:
        explicit USART(I2CMux* b)
            : Protocols::AbstractUsart<I2CMux, Gpio>{b}
        {}

        void write(Byte) override {}

        Byte read() override
        {
            auto& buf =
                (m_activeChannel == 0)
                ? this->m_board->m_buffer0
                : this->m_board->m_buffer1;

            if(buf.empty())
                return 0;

            Byte b = buf.front();
            buf.pop();
            return b;
        }

        void run(Gpio& gpio) override
        {
            gpio.tx.setCanRead(false);
            gpio.rx.setCanRead(true);

            while(gpio.rx.hasByteToRead())
            {
                Byte ch = gpio.rx.read();
                m_activeChannel = ch & 0xFF;

                std::cout
                    << "[MUX USART] channel select: "
                    << (int)m_activeChannel
                    << std::endl;

                if(m_activeChannel == 0)
                {
                    this->m_board->i2c1.disable();
                    this->m_board->i2c0.init(0x29);
                }
                else
                {
                    this->m_board->i2c0.disable();
                    this->m_board->i2c1.init(0x29);
                }
            }

            auto& buf =
                (m_activeChannel == 0)
                ? this->m_board->m_buffer0
                : this->m_board->m_buffer1;

            while(!buf.empty())
            {
                gpio.tx.write(buf.front());
                buf.pop();
            }
        }

    private:
        Byte m_activeChannel = 0;

    } mutable usart{this};

    class I2C : public Protocols::AbstractI2C<I2CMux, Gpio>
    {
    public:
        explicit I2C(I2CMux* b, Byte ch)
            : Protocols::AbstractI2C<I2CMux, Gpio>{b},
            m_buffer(
                ch == 0
                ? b->m_buffer0
                : b->m_buffer1
            ),
            m_channel(ch)
        {}

        void init(Byte address) override
        {
            m_targetAddress = address;
            m_enabled = true;
            this->m_started = true;
            m_state = 0;
            m_rxEnabled = false;

            while(!m_buffer.empty())
                m_buffer.pop();

            std::cout
                << "[MUX" << (int)m_channel
                << "] init target=0x"
                << std::hex
                << (int)m_targetAddress
                << std::dec
                << std::endl;
        }

        void disable()
        {
            auto& sda =
                (m_channel == 0)
                ? this->m_board->gpio().sda0
                : this->m_board->gpio().sda1;

            sda.setCanRead(false);

            while(sda.hasBitToRead())
                sda.readBit();

            while(sda.hasByteToRead())
                sda.read();

            m_enabled = false;
            m_state = 3;
            m_rxEnabled = false;
            this->m_started = false;
        }
        
        void write(Byte byte) override
        {
            m_buffer.push(byte);
        }

        Byte read() override
        {
            if(m_buffer.empty())
                return 0;

            Byte b = m_buffer.front();
            m_buffer.pop();
            return b;
        }

        void run(Gpio& gpio) override
        {
            if(!m_enabled)
                return;
            
            auto& sda =
                (m_channel == 0)
                ? gpio.sda0
                : gpio.sda1;

                
            switch(m_state)
            {
                case 0:
                {
                    if(this->m_started)
                    {
                        sda.setCanRead(false);

                        while(sda.hasBitToRead())
                            sda.readBit();

                        sda.write(m_targetAddress);

                        this->m_started = false;
                        m_state = 1;
                        m_rxEnabled = false;
                    }
                    break;
                }

                case 1:
                {
                    if(sda.hasBitToWrite())
                        return;

                    if(!m_rxEnabled)
                    {
                        sda.setCanRead(true);
                        m_rxEnabled = true;
                        return;
                    }

                    while(sda.hasBitToRead())
                    {
                        Bit ack = sda.readBit();

                        if(ack == Bit::One)
                        {
                            std::cout << "[MUX] received ACK" << std::endl;
                            m_state = 2;
                            std::cout << "[MUX] transition S1 -> S2" << std::endl;
                            break;
                        }
                    }

                    break;
                }

                case 2:
                {
                    while(sda.hasByteToRead())
                    {
                        Byte b = sda.read();

                        std::cout
                            << "[MUX" << (int)m_channel
                            << "] got 0x"
                            << std::hex
                            << (int)(b & 0xFF)
                            << std::dec
                            << std::endl;

                        m_buffer.push(b);

                        sda.setCanRead(false);

                        // flush trailing ACK / residual bits
                        while(sda.hasBitToRead())
                            sda.readBit();

                        m_state = 3;
                        break;
                    }
                    break;
                }

                case 3:
                default:
                {
                    sda.setCanRead(false);
                    break;
                }
            }
        }

    private:
        std::queue<Byte>& m_buffer;

        Byte m_channel;
        Byte m_targetAddress = 0;

        int m_state = 3;
        bool m_rxEnabled = false;
        bool m_enabled = false;
    };
    mutable I2C i2c0 {this, 0};
    mutable I2C i2c1 {this, 1};

protected:
    std::queue<Byte> m_buffer0;
    std::queue<Byte> m_buffer1;

    inline void startModule() override
    {
    }
};

} // namespace Sensors

#endif // I2CMUX_H