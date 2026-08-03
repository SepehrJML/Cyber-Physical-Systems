#ifndef MUX_H
#define MUX_H

#include <CPS4042/Hardwares/Comm/I2CMux.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <iostream>

class MUX : public AbstractSketch<Sensors::I2CMux>
{
public:
    explicit MUX(Sensors::I2CMux* node)
        : AbstractSketch<Sensors::I2CMux>{node}
    {}

    std::int32_t setup(Sensors::I2CMux::Gpio&) override
    {
        std::cout << "MUX setup completed." << std::endl;
        return 0;
    }

    std::int32_t loop(Sensors::I2CMux::Gpio&) override
    {
        while(node()->i2c0.isDataAvailable())
        {
            Byte b = node()->i2c0.read();

            std::cout
                << "[MUX] CH0 received "
                << std::hex
                << (int)(b & 0xFF)
                << std::dec
                << std::endl;

            node()->usart.write(b);
        }

        while(node()->i2c1.isDataAvailable())
        {
            Byte b = node()->i2c1.read();

            std::cout
                << "[MUX] CH1 received "
                << std::hex
                << (int)(b & 0xFF)
                << std::dec
                << std::endl;

            node()->usart.write(b);
        }

        return 0;
    }
};

#endif // MUX_H