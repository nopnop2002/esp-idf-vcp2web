# esp-idf-vcp2web
This project is a wireless VCP(Virtual COM Port) monitor using the ESP32 that can be accessed remotely via a web browser.   
![Image](https://github.com/user-attachments/assets/1a02aa3f-b680-4265-bf5c-1f17a03b0078)

ESP-IDF supports VCP hosts.   
Representative VCP devices include Arduino Uno and Arduino Mega.   
ESP-IDF can communicate with VCP devices using the USB port.   
I based it on [this](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/cdc/cdc_acm_vcp).   

This project uses the following components.   
Other UART-USB converter chips are not supported.   
- https://components.espressif.com/components/espressif/usb_host_ch34x_vcp   
- https://components.espressif.com/components/espressif/usb_host_cp210x_vcp   
- https://components.espressif.com/components/espressif/usb_host_ftdi_vcp   

I used [this](https://github.com/Molorius/esp32-websocket) component.   
This component can communicate directly with the browser.   
It's a great job.   
I referred to [this](https://github.com/ayushsharma82/WebSerial).   
![Web-Serial](https://user-images.githubusercontent.com/6020549/204442158-0e8e1b11-caa8-4937-b830-99d331ca3fa6.jpg)   

# Software requirements
ESP-IDF V5.0 or later.   
ESP-IDF V4.4 release branch reached EOL in July 2024.   

# Hardtware requirements

## ESP32-S2/S3
This project only works with ESP32S2/S3.   
The ESP32S2/S3 has USB capabilities.   

## USB Type-A Femail connector
USB connectors are available from AliExpress or eBay.   
I used it by incorporating it into a Universal PCB.   
![USBConnector](https://github.com/user-attachments/assets/8d7d8f0a-d289-44b8-ae90-c693a1099ca0)

We can buy this breakout on Ebay or AliExpress.   
![usb-conector-11](https://github.com/user-attachments/assets/848998d4-fb0c-4b4f-97ae-0b3ae8b8996a)
![usb-conector-12](https://github.com/user-attachments/assets/6fc34dcf-0b13-4233-8c71-07234e8c6d06)

# Installation
```
git clone https://github.com/nopnop2002/esp-idf-vcp2web
cd esp-idf-vcp2web/
idf.py set-target {esp32s2/esp32s3}
idf.py menuconfig
idf.py flash
```

# Configuration
![Image](https://github.com/user-attachments/assets/370e38e9-5ffd-472a-90fc-0a7b95ecb1d3)
![Image](https://github.com/user-attachments/assets/55eecf46-1eb4-4573-bef4-887cee7f6895)

## WiFi Setting
Set the information of your access point.   
![Image](https://github.com/user-attachments/assets/d9e06dc6-82cf-466a-8209-40fdf59ee5b6)

## NTP Setting
Set the information of your time zone.   
![Image](https://github.com/user-attachments/assets/d99edaee-38e1-4113-b9b0-9cc1485abe2d)

## VCP Setting
Set the information of VCP Connection.   
![Image](https://github.com/user-attachments/assets/1e9d21a3-5206-49e3-8cab-751b7eb8dfe2)

# Write this sketch on Arduino Uno.   
You can use any AtMega microcontroller that has a USB port.   
```
unsigned long lastMillis = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  while (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    Serial.println(command);
  }

  if(lastMillis + 1000 <= millis()){
    Serial.print("Hello World ");
    Serial.println(millis());
    lastMillis += 1000;
  }

  delay(1);
}
```

Strings from Arduino to ESP32 are terminated with CR(0x0d)+LF(0x0a).   
This project will remove the termination character and send to Browser.   
```
I (1189799) UART-RX: 0x3ffc8458   48 65 6c 6c 6f 20 57 6f  72 6c 64 20 31 30 30 31  |Hello World 1001|
I (1189799) UART-RX: 0x3ffc8468   0d 0a
```

The Arduino sketch inputs data with LF as the terminator.   
So strings from the ESP32 to the Arduino must be terminated with LF (0x0a).   
If the string output from the ESP32 to the Arduino is not terminated with LF (0x0a), the Arduino sketch will complete the input with a timeout.   
The default input timeout for Arduino sketches is 1000 milliseconds.   
Since the string received from the Browser does not have a trailing LF, this project will add one to the end and send it to Arduino.   
The Arduino sketch will echo back the string it reads.   
```
I (1285439) UART-TX: 0x3ffc72f8   61 62 63 64 65 66 67 0a                           |abcdefg.|
I (1285459) UART-RX: 0x3ffc8458   61 62 63 64 65 66 67 0d  0a                       |abcdefg..|
```

# Wireing   
Arduino Uno connects via USB connector.   
The USB port on the ESP32S2/S3 development board does not function as a USB-HOST.   

```
+---------+  +-------------+  +-----------+
|ESP BOARD|==|USB CONNECTOR|==|Arduino Uno|
+---------+  +-------------+  +-----------+
```

```
ESP BOARD          USB CONNECTOR (type A)
                         +--+
5V        -------------> | || VCC
[GPIO19]  -------------> | || D-
[GPIO20]  -------------> | || D+
GND       -------------> | || GND
                         +--+
```

![Image](https://github.com/user-attachments/assets/7bf405af-b1ec-4c7c-87d1-8bbe176e807b)

# Launch a web browser   
Enter the following in the address bar of your web browser.   
You can communicate to Arduino-UNO using browser.   
The Change button changes the number of lines displayed.   
The Copy button copies the received data to the clipboard.   
```
http:://{IP of ESP32}/
or
http://esp32-server.local/
```

![Web-Serial](https://user-images.githubusercontent.com/6020549/204442158-0e8e1b11-caa8-4937-b830-99d331ca3fa6.jpg)

# WEB Pages
WEB Pages are stored in the html folder.   
I used [this](https://bulma.io/) open source css.   
You cam change root.html as you like.   
