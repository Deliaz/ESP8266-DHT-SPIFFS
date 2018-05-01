## ESP8266 & DHT22 & SPIFFS
 
Demo for a ESP8266 board with sensor DHT22 and storing static web files using SPIFFS. 
I tested it on Wemos D1 mini (V2). 

This code is built using: 
* [FSBrowser](https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/FSBrowser/FSBrowser.ino)
* [HttpBasicAuth](https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/HttpBasicAuth/HttpBasicAuth.ino)
* [DHT_Unified_Sensor](https://github.com/adafruit/DHT-sensor-library/blob/master/examples/DHT_Unified_Sensor/DHT_Unified_Sensor.ino)

There is some debounce time for sensor updates which implemented using great [CreateTimer](http://forum.arduino.cc/index.php?topic=351306.msg2431155#msg2431155) lib.

### Basic Auth
In order to protect `/edit` endpoint (see FSBrowser example) I use Basic Auth.

### Run 
1. Create your own `credentials.h` from `credentials-example.h` and change settings. 
2. Upload code
3. Upload static files into SPIFFS ([docs](https://github.com/esp8266/arduino-esp8266fs-plugin))
4. Open board's IP with port `8080`
