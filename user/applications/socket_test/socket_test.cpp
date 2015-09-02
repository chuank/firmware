#include "application.h"
#include "modem/mdm_hal.h"

extern MDMElectronSerial electronMDM;

SYSTEM_MODE(MANUAL);

#define now() millis()
uint32_t lastFlash = now();
String com = "";
#define RGB_GREEN() RGB.color(0,255,0)
#define RGB_RED() RGB.color(255,0,0)
#define RGB_BLUE() RGB.color(0,0,255)
#define RGB_OFF() RGB.color(0,0,0)
#define _BKPT __ASM("bkpt 0")
#ifndef UBLOX_PHONE_NUM
	#define UBLOX_PHONE_NUM "5555555555"
#endif

int ret;
#ifdef LARGE_DATA
    char buf[2048] = "";
#else
    char buf[512] = "";
#endif

//------------------------------------------------------------------------------------
// You need to configure these cellular modem / SIM parameters.
// These parameters are ignored for LISA-C200 variants and can be left NULL.
//------------------------------------------------------------------------------------
//! Set your secret SIM pin here (e.g. "1234"). Check your SIM manual.
#define SIMPIN      NULL
/*! The APN of your network operator SIM, sometimes it is "internet" check your
    contract with the network operator. You can also try to look-up your settings in
    google: https://www.google.com/search?q=APN+list */
#define APN         "broadband"
//! Set the user name for your APN, or NULL if not needed
#define USERNAME    NULL
//! Set the password for your APN, or NULL if not needed
#define PASSWORD    NULL

void setup()
{
	RGB.control(true);

#ifndef USE_SWD_JTAG
	pinMode(D7, OUTPUT);
#endif

	Serial.begin(9600);

	//ALL_LEVEL, TRACE_LEVEL, DEBUG_LEVEL, WARN_LEVEL, ERROR_LEVEL, PANIC_LEVEL, NO_LOG_LEVEL
	//Serial1DebugOutput debugOutput(115200, ALL_LEVEL);

	Serial.println("\e[0;36mHi, I'm your friendly neighborhood Electron!");

	// TEST RGB LED
	RGB_RED();
	delay(500);
	RGB_GREEN();
	delay(500);
	RGB_BLUE();
	delay(500);
	RGB_OFF();
	delay(500);

    //electronMDM.setDebug(4); // enable this for debugging issues
    MDMParser::DevStatus devStatus = {};
	MDMParser::NetStatus netStatus = {};
    bool mdmOk = electronMDM.init(SIMPIN, &devStatus);

    //electronMDM.dumpDevStatus(&devStatus);
    if (mdmOk) {
        RGB_GREEN();
        Serial.println("Wait until we are connected to the tower.");
        mdmOk = electronMDM.registerNet(&netStatus);
        //electronMDM.dumpNetStatus(&netStatus);
    }
    if (mdmOk)
    {
    	RGB_BLUE();
    	Serial.println("Establish a PDP context.");
    	//mdmOk = electronMDM.pdp();
    }
    if (mdmOk)
    {
        // http://www.geckobeach.com/cellular/secrets/gsmcodes.php
        // http://de.wikipedia.org/wiki/USSD-Codes
/*
        const char* ussd = "*130#"; // You may get answer "UNKNOWN APPLICATION"
        Serial.print("Ussd Send Command: ");
        Serial.println(ussd);
        ret = electronMDM.ussdCommand(ussd, buf);
        if (ret > 0) {
            Serial.print("Ussd Got Answer: ");
        	Serial.print(ret);
        	Serial.print(" ");
        	Serial.println(buf);
        }
*/
        RGB_RED();
        // join the internet connection
        Serial.println("Make a GPRS connection.");
        MDMParser::IP ip = electronMDM.join(APN,USERNAME,PASSWORD);
        if (ip != NOIP)
        {
            RGB_GREEN();
            //electronMDM.dumpIp(ip);
            Serial.println("Make a Http Post Request.");
            int socket = electronMDM.socketSocket(MDMParser::MDM_IPPROTO_TCP);
            if (socket >= 0)
            {
                electronMDM.socketSetBlocking(socket, 10000);
                if (electronMDM.socketConnect(socket, "mbed.org", 80))
                {
                    const char http[] = "GET /media/uploads/mbed_official/hello.txt HTTP/1.0\r\n\r\n";
                    electronMDM.socketSend(socket, http, sizeof(http)-1);

                    ret = electronMDM.socketRecv(socket, buf, sizeof(buf)-1);
                    if (ret > 0) {
                        Serial.print("Socket Recv: ");
                    	Serial.print(ret);
                    	Serial.print(" ");
        				Serial.println(buf);
        			}
                    electronMDM.socketClose(socket);
                }
                electronMDM.socketFree(socket);
            }

            int port = 7;
            const char* host = "echo.u-blox.com";
            MDMParser::IP ip = electronMDM.gethostbyname(host);
            char data[] = "\r\nxxx Socket Hello World\r\n"
#ifdef LARGE_DATA
            "00  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "01  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "02  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "03  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "04  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"

            "05  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "06  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "07  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "08  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "09  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"

            "10  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "11  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "12  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "13  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "14  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"

            "15  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "16  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "17  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "18  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
            "19  0123456789 0123456789 0123456789 0123456789 0123456789 \r\n"
#endif
            "End\r\n";

            Serial.print("Testing TCP sockets with ECHO server\r\n");
            socket = electronMDM.socketSocket(MDMParser::MDM_IPPROTO_TCP);
            if (socket >= 0)
            {
                electronMDM.socketSetBlocking(socket, 10000);
                if (electronMDM.socketConnect(socket, host, port)) {
                    memcpy(data, "\r\nTCP", 5);
                    ret = electronMDM.socketSend(socket, data, sizeof(data)-1);
                    if (ret == sizeof(data)-1) {
                        Serial.print("Socket Send: ");
                        Serial.print(ret);
                        Serial.print(" ");
                        Serial.println(data);
                    }
                    ret = electronMDM.socketRecv(socket, buf, sizeof(buf)-1);
                    if (ret >= 0) {
                        Serial.print("Socket Recv: ");
                        Serial.print(ret);
                        Serial.print(" ");
                        Serial.println(buf);
                    }
                    electronMDM.socketClose(socket);
                }
                electronMDM.socketFree(socket);
            }

            Serial.print("Testing UDP sockets with ECHO server\r\n");
            socket = electronMDM.socketSocket(MDMParser::MDM_IPPROTO_UDP, port);
            if (socket >= 0)
            {
                electronMDM.socketSetBlocking(socket, 10000);
                memcpy(data, "\r\nUDP", 5);
                ret = electronMDM.socketSendTo(socket, ip, port, data, sizeof(data)-1);
                if (ret == sizeof(data)-1) {
                    Serial.print("Socket SendTo: ");
                    Serial.print(host);
                    Serial.print(":");
                    Serial.print(port);
                    Serial.print(" IPSTR ");
                    Serial.print(0);//IPNUM(ip));
                    Serial.print(" ");
                    Serial.print(ret);
                    Serial.print(":");
                    Serial.println(data);
                }
                ret = electronMDM.socketRecvFrom(socket, &ip, &port, buf, sizeof(buf)-1);
                if (ret >= 0) {
                    Serial.print("Socket RecvFrom IPSTR: ");
                    Serial.print(0);//IPNUM(ip));
                    Serial.print(":");
                    Serial.print(port);
                    Serial.print(" ");
                    Serial.print(ret);
                    Serial.print(":");
                    Serial.println(buf);
                }
                electronMDM.socketFree(socket);
            }

            // disconnect
            electronMDM.disconnect();
        }
    }
}

void loop()
{
	RGB_OFF();
#ifndef USE_SWD_JTAG
	// TEST USER LED
	if ( now() - lastFlash > 100UL ) {
		lastFlash = now();
		digitalWrite(D7, !digitalRead(D7));
	}
#endif
}
