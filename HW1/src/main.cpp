#include <CPS4042/Sketchs/HardDisk.h>

#include <CPS4042/Hardwares/Comm/I2CMux.h>
#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Hardwares/Boards/Esp8266.h>

#include <CPS4042/Sketchs/Microcontroller.h>
#include <CPS4042/Sketchs/Mux.h>
#include <CPS4042/Sketchs/Sensor.h>

#include <CPS4042/main.h>

std::int32_t main(int argc, char* argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "--i2c") == 0)
    {
        Boards::Esp8266 esp8266;
        Sensors::Vl530x vl530x{Sensors::Vl530x::Mode::I2C};

        auto linkRed    = std::make_shared<Link>();
        auto linkBlack  = std::make_shared<Link>();
        auto linkGreen  = std::make_shared<Link>();
        auto linkYellow = std::make_shared<Link>();

        esp8266.gpio().vdd1.attachLink(linkRed);
        esp8266.gpio().gnd1.attachLink(linkBlack);

        esp8266.gpio().scl.attachLink(linkGreen);
        esp8266.gpio().sda.attachLink(linkYellow);

        vl530x.gpio().vdd.attachLink(linkRed);
        vl530x.gpio().gnd.attachLink(linkBlack);

        vl530x.gpio().scl.attachLink(linkGreen);
        vl530x.gpio().sda.attachLink(linkYellow);

        MicroController micro(
            &esp8266,
            MicroController::Mode::I2C
        );

        Sensor disSen(
            &vl530x,
            Sensor::Mode::I2C
        );

        micro.start();
        disSen.start();

        return Application::exec();        
    }
    else if (argc > 1 && std::strcmp(argv[1], "--usart") == 0)
    {
        Boards::Esp8266 esp8266;
        Sensors::Usb usb;
    
        auto linkRed = std::make_shared<Link>();
        auto linkBlack = std::make_shared<Link>();
    
        auto linkTx = std::make_shared<Link>();
        auto linkRx = std::make_shared<Link>();
    
        CPS_SET_OBJECT_NAME(esp8266);
        CPS_SET_OBJECT_NAME(usb);
    
        CPS_SET_OBJECT_NAME_PTR(linkRed);
        CPS_SET_OBJECT_NAME_PTR(linkBlack);
        CPS_SET_OBJECT_NAME_PTR(linkTx);
        CPS_SET_OBJECT_NAME_PTR(linkRx);
    
        esp8266.gpio().vdd1.attachLink(linkRed);
        esp8266.gpio().gnd1.attachLink(linkBlack);
    
        usb.gpio().vdd.attachLink(linkRed);
        usb.gpio().gnd.attachLink(linkBlack);
    
        esp8266.gpio().tx.attachLink(linkTx);
        usb.gpio().rx.attachLink(linkTx);
    
        esp8266.gpio().rx.attachLink(linkRx);
        usb.gpio().tx.attachLink(linkRx);
    
        MicroController micro(&esp8266,MicroController::Mode::USART);
        HardDisk hardDisk(&usb);
    
        micro.start();
        hardDisk.start();
    
        return Application::exec();     
    }
    else if (argc > 1 && std::strcmp(argv[1], "--mux") == 0)
    {
        Boards::Esp8266 esp8266;

        Sensors::I2CMux mux;
    
        Sensors::Vl530x vl530xOne{Sensors::Vl530x::Mode::MUX};
        Sensors::Vl530x vl530xTwo{Sensors::Vl530x::Mode::MUX};
    
    
        auto linkRed1   = std::make_shared<Link>();
        auto linkBlack1 = std::make_shared<Link>();
    
        auto linkRed2   = std::make_shared<Link>();
        auto linkBlack2 = std::make_shared<Link>();
    
        auto linkRed3   = std::make_shared<Link>();
        auto linkBlack3 = std::make_shared<Link>();
    
        auto linkTx = std::make_shared<Link>();
        auto linkRx = std::make_shared<Link>();
    
        auto linkScl0 = std::make_shared<Link>();
        auto linkSda0 = std::make_shared<Link>();
    
        auto linkScl1 = std::make_shared<Link>();
        auto linkSda1 = std::make_shared<Link>();
    
    
        esp8266.gpio().vdd1.attachLink(linkRed1);
        esp8266.gpio().gnd1.attachLink(linkBlack1);
    
        mux.gpio().vdd.attachLink(linkRed1);
        mux.gpio().gnd.attachLink(linkBlack1);
    
    
        esp8266.gpio().vdd1.attachLink(linkRed2);
        esp8266.gpio().gnd1.attachLink(linkBlack2);
    
        vl530xOne.gpio().vdd.attachLink(linkRed2);
        vl530xOne.gpio().gnd.attachLink(linkBlack2);
    
    
        esp8266.gpio().vdd1.attachLink(linkRed3);
        esp8266.gpio().gnd1.attachLink(linkBlack3);
    
        vl530xTwo.gpio().vdd.attachLink(linkRed3);
        vl530xTwo.gpio().gnd.attachLink(linkBlack3);
    
    
        esp8266.gpio().tx.attachLink(linkTx);
        mux.gpio().rx.attachLink(linkTx);
    
        esp8266.gpio().rx.attachLink(linkRx);
        mux.gpio().tx.attachLink(linkRx);
    
    
        
        mux.gpio().sda0.attachLink(linkSda0);
        vl530xOne.gpio().sda.attachLink(linkSda0);
    
        mux.gpio().scl0.attachLink(linkScl0);
        vl530xOne.gpio().scl.attachLink(linkScl0);
    
    
        
        mux.gpio().sda1.attachLink(linkSda1);
        vl530xTwo.gpio().sda.attachLink(linkSda1);
    
        mux.gpio().scl1.attachLink(linkScl1);
        vl530xTwo.gpio().scl.attachLink(linkScl1);
    
    
        
        MicroController micro(
            &esp8266,
            MicroController::Mode::Mux
        );
    
        MUX muxSketch(&mux);
    
        Sensor disSen0(
            &vl530xOne,
            Sensor::Mode::Mux,
            0
        );
    
        Sensor disSen1(
            &vl530xTwo,
            Sensor::Mode::Mux,
            1
        );
    
    
        
        micro.start();
        muxSketch.start();
    
        disSen0.start();
        disSen1.start();
        
    
        return Application::exec();     
    }
    else 
    {
        std::cout << "Usage: .\\CPS4042.exe <mode>\n"
        << "mode = ['--i2c','--usart','--mux']\n";
        return 1;

    }
}