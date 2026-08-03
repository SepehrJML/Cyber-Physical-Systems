#ifndef SENSOR_H
#define SENSOR_H

#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <CPS4042/Units/Byte.h>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <iostream>
#include <cstdlib>
#include <algorithm>

class Sensor : public AbstractSketch<Sensors::Vl530x>
{
public:
    enum class Mode
    {
        Mux,
        I2C
    };

    explicit Sensor(
        Sensors::Vl530x* node,
        Mode mode = Mode::Mux,
        int base = 0
    )
        : AbstractSketch<Sensors::Vl530x>{node},
          m_mode(mode),
          m_base(base)
    {
        std::cout << "Sensor sketch created.\n";
    }

    std::int32_t setup(Sensors::Vl530x::Gpio&) override
    {
        std::cout << "vl530x setup completed.\n";
        node()->i2c.init(0);
        return 0;
    }

    std::int32_t loop(Sensors::Vl530x::Gpio&) override
    {
        if(m_mode == Mode::Mux)
            return muxLoop();

        return i2cLoop();
    }

private:

    /*
    =========================
        MUX MODE
    =========================
    */

    std::int32_t muxLoop()
    {
        if(!node()->i2c.isDataAvailable())
        {
            Byte value;

            if(m_base == 0)
                value = rand() % 21;
            else
                value = 50 + rand() % 51;

            std::cout
                << "[Sensor " << m_base
                << "] sending "
                << (int)value
                << std::endl;

            node()->i2c.write(value);
        }

        return 0;
    }

    /*
    =========================
        DIRECT I2C MODE
    =========================
    */

    std::int32_t i2cLoop()
    {
        if(!node()->i2c.isDataAvailable())
        {
            static boost::random::mt19937 rng(42);

            static boost::random
                ::uniform_int_distribution<>
                    dist(0, 4000);

            std::uint16_t distanceVal =
                dist(rng);

            Byte b1 =
                getByte<1>(distanceVal);

            Byte b2 =
                getByte<0>(distanceVal);

            UByte ub1 =
                static_cast<UByte>(b1);

            UByte ub2 =
                static_cast<UByte>(b2);

            Byte checksum =
                static_cast<Byte>(
                    std::max(ub1, ub2)
                    -
                    std::min(ub1, ub2)
                );

            node()->i2c.write(b1);
            node()->i2c.write(b2);
            node()->i2c.write(checksum);

            std::cout
                << "[Sensor] distance="
                << distanceVal
                << " checksum="
                << (int)(checksum & 0xFF)
                << std::endl;
        }

        return 0;
    }

private:
    Mode m_mode;
    int m_base;
};

#endif // SENSOR_H