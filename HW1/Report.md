## CPS CA1 Report - Spring 1405

### Team Members
| Name | Student ID | Contribution |
|---|---|---|
| Sepehr Bazargan | 810102303 | I2C MUX |
| Sepehr Jamali | 810101400 | I2C MUX |
| Mohamad Mahdi Sharifi | 810101449 | I2C |
| Mohamad Mahdi Samadi | 810101465 | USART |

---

## I2C
### Qestions
**Q1. In the standard I2C bus protocol, what issue occurs if more than one sensor with the same address is connected?**
In I2C, the Master initiates communication by sending a specific 7-bit address. If two sensors share the same address, both will attempt to drive the SDA (data) line simultaneously. When one sensor tries to send a '0' and the other a '1', it leads to **bus contention**. This causes data corruption and makes it impossible for the Master to distinguish between the two devices.

**Q2. List four general shortcomings of this simplified simulation protocol compared to the standard I2C.**

- **Lack of Multi-Master Support:** The simulation assumes a single Master, whereas standard I2C supports multiple masters using arbitration.
- **No Clock Stretching:** In the standard, a Slave can hold the SCL line low to slow down the Master; this simulation protocol does not support this flow control.
- **Fixed Speed/Timing:** Standard I2C has defined modes (Standard, Fast, High-speed), while the simulation operates on simplified cycle-based logic.
- **Error Handling:** Standard I2C uses specific ACK/NACK bit transitions for every byte; a simplified simulation might skip these granular state checks for ease of implementation.
- **No Stop Condition:** Real I2C uses dedicated STOP condition. In this project, the ACK bit indicates start of sending data but there's no specific sign of end to it.


**Q3. Explain the function of the following classes and functions: `ByteStream`, `ByteVector`, and `getByte`.**
* **`ByteStream`:** Acts as a data buffer that manages a sequence of bytes, allowing for checking if a complete packet (like a 2-byte integer) is ready for processing via `.isReady()`.
* **`ByteVector`:** A specialized container (likely based on `std::vector`) used to store and manipulate a collection of bytes for transmission or storage.
* **`getByte`:** Utility function defined on `Byte.h` header file used to return the byte stores at a specific index of some ByteVector. It has three different method signatures overloading it, each to use on necessary cases.

**Q3: Explain the function of the following classes and functions: `ByteStream`, `ByteVector`, and `getByte`.**
* **`ByteStream`:** A template class that collects incoming `Byte` objects (using the `<<` operator) into a larger integral value. It uses `m_count` to track progress and `isReady()` to signal when the full width of the data type (e.g., a 16-bit sensor value) has been received.
* **`ByteVector`:** A wrapper for an integral value that provides a `BytePtr` to its underlying memory. It allows treating a multi-byte variable (like an `int`) as an array of `Byte` units via the `[]` operator.
* **`getByte`:** A helper function that uses `reinterpret_cast` to access a specific byte index within an integral value. It ensures safe access by checking that the requested index is within the size of the data type.

### Implementation Report
ESP8266.h
```cpp
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
    
protected:
    inline void startModule() override {}
};
```
*Code explanation:*
> The nested I2C class implements the ESP8266’s role as an I2C master, responsible for initiating communication with a target sensor and receiving measurement data.
>
> When the init() function is called, the target sensor address is stored, the protocol is marked as active, the internal state machine is reset, and any previously buffered data is cleared. This prepares the ESP8266 to start a fresh transaction.
> 
> The communication process is handled by a finite-state machine inside run():
> 
> - State 0 (Start Transmission):
> When communication begins, SDA reading is temporarily disabled and any stale bits are cleared from the bus. The ESP8266 then writes the target sensor address to SDA, signaling the start of an I2C read transaction, and transitions to the acknowledgment state.
> - State 1 (Wait for ACK):
> The controller waits until address transmission is complete. It then enables SDA reading and listens for an acknowledgment bit from the sensor. Once an ACK (Bit::One) is received, communication proceeds to the data reception state.
> - State 2 (Receive Data):
> The ESP8266 reads incoming bytes from SDA and stores them in its internal buffer. It expects exactly three bytes, corresponding to the simulated sensor measurement packet. Once all three bytes are received, reading is disabled and the protocol moves to the final state.
> - State 3 (Idle):
> Communication is terminated by disabling SDA reads, leaving the bus ready for the next polling cycle.
> 
> The behavior of the I2C controller is managed using several private variables:
> 
> - m_state: stores the current state of the I2C finite-state machine, controlling which communication step is active.
> - m_bytesRead: counts how many data bytes have been received during the current transaction. It is used to stop reception after the expected three-byte packet is complete.
> - m_rxEnabled: tracks whether SDA reading has already been enabled while waiting for acknowledgment, preventing repeated reconfiguration of the bus line.
> - m_targetAddress: stores the I2C address of the sensor currently being polled, allowing communication with the correct device.

VL530x.h
```cpp
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
```
*Code explanation:*
> The Vl530x sensor class supports multiple operating modes through the Mode enum, where I2C is the default mode. When an object is created, the selected mode is stored in m_mode, allowing the internal protocol logic to adapt its timing and communication behavior depending on the selected interface.
> 
> In I2C mode, communication is handled by the nested I2C protocol class, which simulates the behavior of a real VL53L0X sensor on an I2C bus. The init() function marks the protocol as active whenever the master starts a transaction and prints a debug message for verification. Incoming data bytes from the microcontroller are stored in an internal buffer through write(), while read() allows buffered bytes to be retrieved sequentially.
> 
> The main communication logic is implemented as a finite-state machine inside run(), which is triggered on every rising edge of the SCL clock line.
> 
> - State 0 (Listening):
> The sensor waits for an incoming address byte on SDA. If the received address matches the sensor’s fixed I2C address (0x29), the sensor disables further reads and transitions to the acknowledgment state.
> - State 1 (ACK):
> In I2C mode, the sensor introduces a short delay of two clock cycles before responding. This prevents timing collisions with the ESP8266 master. After the delay, the sensor writes an ACK bit to SDA and transitions to the transmit state.
> - State 2 (Transmit Data):
> The sensor waits until SDA is free for writing. It then sends all buffered bytes sequentially over the bus, simulating sensor measurement output. Once transmission completes, the protocol transitions to the cleanup state.
> - State 3 (Bus Reset):
> The sensor waits for all outgoing bits to finish transmission, re-enables SDA reading, clears any remaining bits on the line, and returns to the listening state for the next transaction.

sensor.h
```cpp
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
```
*Code explanation:*
> Here we just have a simple loop, to generate the data specified in the project, we will generate a random number between 1-4000, serialize it into 3 bytes, b1, b2, checksum, and the written into the i2c bus using `node()->i2c.write()`.

Microcontroller.h
```cpp
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
```
*Code explanation:*
> This loop is acting as the esp8266, polling periodically a 3-byte value from the sensor, every 1000ms, we disable the SDA reads, we pop all the bits and any pending `node()->i2c` bytes, clearing the `distanceStream`, after that we start another transaction between microcontroller and the sensor, by `node()->i2c.init(0x29)`. the bytes are read in a loop, the first two bytes are data and the last one is a checksum. the checksum logic is implemented as specified in the project doc, then a log is produced, if ok we print the new distance if not, we will print that the checksum was invalid. The first two bytes will be pushed into `distanceStream`, which is a 16-bit value in mm, which later will be printed as the distance.
---

## USART

### Questions

**Q1. Why are the Start and Stop bits not mistaken for data bits?**
USART uses time-based framing. The receiver remains in an Idle state while the line is High. The **Start Bit** is defined as the first transition to Low. Because the receiver knows the Baud Rate and the frame size (8 bits), it ignores any transitions after the 8th bit until it sees the High **Stop Bit**, which resets the state machine for the next frame.


**Q2. Since there is no clock line (Asynchronous), how does the protocol detect data?**
It relies on a shared `BaudRate` configuration. The receiver triggers a timer on the falling edge of the Start Bit and samples the line at the midpoint of each subsequent bit period. The `Board` class uses these predefined rates to ensure the Master and Slave are synchronized in time rather than by a physical wire.


**Q3. Can USART be implemented via software (Bit-Banging) on digital pins without dedicated hardware? Compare this to I2C.**
Yes, it can, but it is much harder than I2C.
* **Comparison:** I2C is synchronous (uses a clock line), so the timing is "pushed" by the master; "bit-banging" is relatively simple.
* **USART Difficulty:** Because USART is asynchronous, software "bit-banging" requires extremely precise, non-blocking delays. If the CPU is busy with another task for even a few microseconds, it will miss the sampling window, leading to "Garbage" data. Hardware UART is preferred because it handles this timing using dedicated shift registers and interrupts.

### Implementation Report
esp8266.h
```cpp
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

                std::cout << "USART RX: 0x"
                        << std::hex
                        << (static_cast<int>(received) & 0xFF)
                        << std::endl;
            }
        }

    } mutable usart {this};
```
*Code explanation:*

>This class implements the USART protocol for the ESP8266 device. It consists of three main functions:
>
>- write(Byte byte) pushes a byte into the protocol buffer so it can be transmitted later.
> - read() retrieves a byte from the buffer. If the buffer is empty, it returns 0.
>- run(Gpio& gpio) is the main routine and is executed every processor cycle.
> Inside run, the TX pin is configured so it cannot be read from, while the RX pin is enabled for reading.
>
> The function first checks whether a transmission is currently in progress. If writing has not finished and there is data in the buffer, it checks whether the TX line is free. If the TX line is available, the next byte is removed from the buffer and written to the TX pin for transmission.
> 
> The m_isWritingFinished flag is used to prevent sending another byte until the current transmission has completed.
>
> Finally, the function checks whether any bytes have been received on the RX pin. If data is available, it reads the bytes and prints them to the console.

Microcontroller.h
```cpp
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

```
*Code explanation:*

> In the loop funcion microcontroller sends a new address through the TX line every second in order to request the corresponding data from the hard disk. The address increases each time the loop triggers.*

Usb.h
```cpp
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

        void run(Gpio& gpio) override {

            gpio.tx.setCanRead(false);
            gpio.rx.setCanRead(true);

            while(gpio.rx.hasByteToRead())
            {
                Byte received = gpio.rx.read(); 
                std::cout << "USB RX: " << std::hex << (static_cast<int>(received) & 0xFF) << std::endl;
                this->m_buffer.push(received);
            }
        }
    } mutable usart {this};

protected:
    inline void startModule() override
    {
    }
};

```
*Code explanation:*
> This class represents the USB board in the system. Its purpose is to receive address requests from the ESP8266 and pass them to the hard disk.
>
> In the constructor, the USB board installs the USART protocol into its processor using m_processor->installProtocol(&usart). This allows the processor to use the USART interface for communication.
>
> Inside the class, a nested USART class implements the USART protocol for the USB board. The write and read functions behave similarly to those in the ESP8266 USART implementation.
>
> The main logic is implemented in the run(Gpio& gpio) function. 
> The function continuously checks whether there are bytes available on the RX pin. If data is received, the byte is read from the RX line and pushed into the internal buffer.
>
>This buffer allows the hard disk module to read the received data later. In this way, the USB receives address requests from the ESP8266 and stores them so the hard disk can access the requested address.

HardDisk.h
```cpp
    std::int32_t
    loop(Sensors::Usb::Gpio& gpio) override {
        auto& usart = m_usb->usart;

        while(usart.isDataAvailable())
        {
            Byte address = usart.read();

            std::cout << "HardDisk received address: " << std::hex << static_cast<int>(address) << std::endl;

            if(m_storage.contains(address))
            {
                Byte data = m_storage[address];
                std::cout << "HardDisk sending data: " << std::hex << (static_cast<int>(data) & 0xFF) << std::endl;
                gpio.tx.write(data);
            }
        }

        return 0;
    }

```
*Code explanation:*
> In the loop function of hard disk it continuously checks whether the USB USART interface has received any data.
>
> First, a reference to the USB USART interface is obtained using auto& usart = m_usb->usart. The function then checks if there is any data available in the USART buffer using usart.isDataAvailable().
>
> If data is available, the hard disk reads a byte from the buffer using usart.read(). This byte represents an address requested by the ESP8266.
>
> After receiving the address, the hard disk checks whether the address exists in its internal storage (m_storage). If the address is present, the corresponding data value is retrieved from the storage.
>
> The hard disk sends this data back to ESP8266 through the TX line using gpio.tx.write(data).

---

## I2C MUX
### Questions
**Q1: Explain the `reverse` function in `Byte.h`. Is this used in real boards?**
The `reverse` function iterates through the `bitWidth` of a byte, taking the $i$-th bit using `takeNthBit` and shifting it into a new result. This is used in real boards to handle **Endianness** or bit-order mismatches between a processor (which might be MSB-first) and a peripheral (which might be LSB-first).

**Q2: Explain the `takeNthBit` functions in `Bit.h`. What is the difference between them?**
The `takeNthBit(value, n)` function takes a runtime variable and extracts the bit at position $n$ using a bitwise mask. It performes a bitwise AND of the given value of size n with a series of bit with all zeros except for the n'th place which is one. At the end the anser is either 0 (if the n'th bit is 0) or 1 (if the n'th bit is 1). The template version `takeNthBit<byte, n>()` performs this at **compile-time** using `static_assert` to ensure the index $n$ is valid for the data type's width, providing better performance and safety.

**Q3: Explain the `attachPinToCommunicationClock` function from the `Board` class.**
This function connects a `Digital` pin to the `communicationClockChanged` signal of the `Processor`. It uses a lambda function `[this, &pin](Bit edge) { pin.write(edge); }` to ensure that whenever the processor's clock toggles, the physical state of the pin is updated automatically, effectively simulating a hardware SCL line. It also checks some static assertions before doing so; like ensuring the frequency is Drived (0b00000000), pin index is greater than pins count and pin type is Digital.



### Implementation Report
I2CMux.h
```cpp
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
```
*Code explanation:*
>It contains two independent I2C interfaces (i2c0 and i2c1) and one USART interface used for channel selection and forwarding sensor data back to the ESP8266.
>
>The USART protocol is responsible for receiving the selected channel number from the ESP8266. Inside run(), the received byte is stored in m_activeChannel, which determines whether channel 0 or channel 1 should be active. When a channel is selected, the opposite channel is disabled using disable(), and the selected channel is initialized using init(0x29). This guarantees that only one sensor is active at a time and prevents bus collisions. The same USART object also forwards received sensor bytes back to the ESP8266 by transmitting the contents of the currently active channel buffer.
>
>The I2C protocol implements the actual sensor transaction state machine for each channel. Each instance is bound to its own queue buffer (m_buffer) and physical SDA/SCL lines based on m_channel.
>
>The init() function prepares a new transaction by:
>
>- storing the target sensor address in m_targetAddress
>- enabling the channel using m_enabled
>resetting the protocol state to m_state = 0
>- clearing any previous buffered bytes
>- disabling receive mode (m_rxEnabled = false)
>
> The disable() function fully resets an inactive channel by clearing pending bits and bytes from its SDA line, disabling reads, and forcing the state machine into idle state (m_state = 3).
>
> The run() function executes the communication through four states:
>
>- State 0 (Address Transmission): 
> The mux clears residual bus bits, disables reading, and writes the sensor address to begin communication. The state then moves to ACK waiting.
>- State 1 (ACK Detection):
> The mux waits until transmission finishes, enables reading, and checks for the ACK bit returned by the sensor. If ACK is detected, communication proceeds to data reception.
>- State 2 (Data Reception):
>The sensor response byte is read from SDA and stored into m_buffer. Residual ACK bits are flushed to keep the bus clean, then the transaction ends.
>- State 3 (Idle / Disabled):
>The channel remains inactive until explicitly reinitialized by the USART channel selector.
>
> The private variables control the protocol flow:
>
>- m_buffer → stores bytes received from the sensor for this channel
>- m_channel → identifies whether this instance controls channel 0 or channel 1
>- m_targetAddress → stores the I2C address being queried (0x29)
>- m_state → tracks current protocol state (0–3)
>- m_rxEnabled → ensures receive mode is enabled only when needed
>- m_enabled → determines whether this channel is currently active


Vl530x.h
```cpp
case 1: 
    else {
        if(m_waitCycle < 20)
        {
        m_waitCycle++;
        return;
        }
    }
case 2:
else {
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
```
*Code explanation:*
> Here only the parts that are different from the original VL530x.h, which was explained in the I2C section, are checked. These differences appear in the else conditions of case 1 and case 2.
> 
> The first change is in case 1, where the wait cycle is increased from 2 to 20. This delay is necessary because, when operating through the multiplexer, channel switching introduces extra latency before communication becomes stable. The additional wait time ensures that the selected channel is fully active before the sensor sends its acknowledgment signal.
> 
> The second change is in case 2, where data transmission behavior is modified. In normal I2C mode, the sensor sends all buffered bytes continuously in a single transaction. In MUX mode, however, only one byte is transmitted per cycle. After sending a single byte, the state machine immediately transitions to state 3 and waits for the bus to stabilize before continuing.

Microcontroller.h
```cpp
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
```
*Code explanation:*
> This loop is used to switch the channel of the MUX, between 0,1 using the USART protocol, basically esp selects the channel using this loop in the microcontroller sketch, so we will continuously switch the channel to read data from both the sensors that are connected to the mux.

Sensor.h
```cpp
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

```
*Code explanation:*
> The loop looks at the sensor number argument, if its 0 a number between 0-20 is generated and written on the sda bus using `node()->i2c.write(value)`, if its 1 a random number between 50-100 is generated and written on the bus.  

Mux.h
```cpp
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
```
*Code explanation:*
> The MUX class represents the software sketch running on the I2C multiplexer module. It inherits from `AbstractSketch<Sensors::I2CMux>`, allowing it to directly control the behavior of the simulated multiplexer hardware. 
> 
> The constructor receives a pointer to the multiplexer node and passes it to the parent sketch class, linking the control logic to the hardware instance.   
> 
> The setup() function is executed once during initialization. Its purpose is to confirm successful startup of the multiplexer module by printing a debug message to the console. 
> 
> The main behavior is implemented inside the loop() function, which continuously monitors both I2C channels of the multiplexer: 
> - Channel 0 Monitoring: 
> The sketch checks whether new data is available on i2c0. If data exists, it is read byte-by-byte, printed to the console for debugging, and forwarded through the USART interface.
> - Channel 1 Monitoring: 
> The same process is repeated for i2c1, allowing the multiplexer to collect data from the second connected sensor.
> 
> This forwarding mechanism allows the multiplexer to act as an intermediate bridge between multiple VL53L0X sensors and the ESP8266. Each sensor communicates over its dedicated I2C channel, while the multiplexer gathers incoming sensor bytes and transmits them through USART to the ESP8266.
---