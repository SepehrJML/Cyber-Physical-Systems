// this code file was added to simulate a USB/hard-disk device

#ifndef USB_H
#define USB_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>

namespace Sensors
{

using UsbVoltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct UsbGpio
{
public:
    Pins::Vdd<WorkingVoltageTp> vdd {BR, BTR, "Usb::vdd"};
    Pins::Gnd<WorkingVoltageTp> gnd {BR, BTR, "Usb::gnd"};

    Pins::Rx<WorkingVoltageTp> rx {BR, BTR, "Usb::rx"};
    Pins::Tx<WorkingVoltageTp> tx {BR, BTR, "Usb::tx"};
};

class Usb : public Board<
    BaudRates::B115200,
    BitRates::same(BaudRates::B115200),
    Frequency::F320khz,
    UsbVoltage,
    UsbGpio>
{
public:
    explicit Usb() : Parent {"Usb::processor"} {
        m_processor->installProtocol(&usart);
        std::cout << "one instance of Usb created." << std::endl;
    }

    // It's here to receive bytes from ESP8266
    class USART : public Protocols::AbstractUsart<Usb, Gpio> {
    public:
        explicit USART(Usb* b)
            : Protocols::AbstractUsart<Usb, Gpio> {b} {}

        void write(Byte byte) override { this->m_buffer.push(byte); }

        Byte read() override {
            if(this->m_buffer.empty())
                return 0;

            Byte b = this->m_buffer.front();
            this->m_buffer.pop();

            return b;
        }

        // runs every processor cycle
        void run(Gpio& gpio) override {
            // Allows USB device to receive data by opening its read end-point and closing its write end-point
            gpio.tx.setCanRead(false);
            gpio.rx.setCanRead(true);

            while(gpio.rx.hasByteToRead())
            {
                Byte received = gpio.rx.read(); // receives data from rx
                std::cout << "USB RX: " << std::hex << (static_cast<int>(received) & 0xFF) << std::endl;
                this->m_buffer.push(received); // stored received data to its buffer
            }
        }
    } mutable usart {this};

protected:
    inline void startModule() override
    {
    }
};

} // namespace Sensors

#endif // USB_H