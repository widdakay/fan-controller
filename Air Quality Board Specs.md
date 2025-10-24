  
Software

* Configuration file  
  * There is one configuration file for all important configurations in this project. It contains  
    * WiFi SSIDs/PSKs  
    * URLs for all APIs  
    * Pins for all IO  
* Watchdog timer  
  * Set to a reasonable few second timeout  
* Boot behavior  
  * Send package of collected data over boot endpoint  
  * List WiFis, signal strengths, and any other interesting info  
  * Record ResetReason and ResetReason2 from ESP API  
  * Connect to strongest WiFi that matches SSID from a list of several SSID/password pairs  
  * Send notification over boot endpoint  
* ArduinoOTA FW Update  
  * Use ArduinoOTA to allow for immediate remote reprogramming  
* HTTPS FW Update  
  * Once on WiFi and once per hour afterwards, the device should check the latest FW version and if a newer one exists, update to it using the ESP API  
  * Send a POST request to [https://data.yoerik.com/fw/update](https://data.yoerik.com/fw/update)  
    * {"ID": ESP Chip ID, "ver": current FW version}  
    * The server will respond with either “True” or “False” if there is an update.  
    * If true, send {"ID": ESP Chip ID, "ver": current FW version, "download": True}  
  * On failure, continue running as usual.   
* Data is sent to InfluxDB through an API vs POST to https://data.yoerik.com/particle/log  
  * Data should be in this format:  
    {"measurement": measurement, "tags": tags, "fields": fields}  
  * All data reports should have the ESP ChipID and a human configured name for the overall sensor board in the tags as well as other information  
  * One field should always be arduino\_millis and contain milliseconds since boot.   
* MQTT server to control motor  
  * server is 10.10.1.20:1883  
  * publish to home/fan1/power/status topic every 10 seconds with a 0-1 value for current fan speed  
  * update fan power based on an incoming 0.0-1.0 text value published by another device on home/fan1/power  
* LEDs  
  * Flash the green led to notify the user that the device is alive  
  * Flash the red led for 500ms every time a web request fails  
  * Flash orange when updating over HTTPS OTA  
  * Turn on blue LED if the motor is on. 

Full pinout:

* Green debug LED: 4  
* Orange debug LED: 5  
* Red debug LED: 6  
* Blue debug LED: 7

* OneWire1: 3  
* OneWire2: 46  
* OneWire3: 9  
* OneWire4: 10

* SDA1: 11  
* SCL1: 12  
* SDA2: 13  
* SCL2: 14  
* SDA3: 21  
* SCL3: 47  
* SDA4: 48  
* SCL4: 45

* Motor IN B: 35  
* Motor EN B: 36  
* Motor PWM: 38  
* Motor EN A: 40  
* Motor IN A: 41

* Debug Serial Tx: TXD0  
* Debug Serial Rx: RXD0

* Onboard I2C SDA: 1  
* Onboard I2C SCL: 2

Board in-built hardware peripherals:  
Onboard I2C has two chips:  
ADS1115 16 bit ADC

* AIN0: Motor NTC thermistor divider node. 10K resistor on bottom. Thermistor on top.  
* AIN1: MCU/board NTC thermistor divider node. 10K resistor on bottom.  Thermistor on top.  
* AIN2: On-board 3.3 V rail via 2:1 divider.  
* AIN3: On-board 5.0 V rail via 2:1 divider.

INA226 with 

* 1 milliohm shunt  
* Measures whole board power at DC input before dc-dc converter

Submit one data report every 5 seconds with board information:  
{"measurement": "ESP\_Health", "tags": standard\_tags, "fields":   
{arduino millis: millis(),  
Motor temp C: AIN0  
MCU external temp C: AIN1  
MCU internal temp C: temperature\_sensor\_get\_celsius()  
3V3 Rail: 3.3v readback  
5V Rail: 5v readback  
V\_IN: INA226 Volts  
I\_IN: INA226 Amps  
V\_SHUNT: INA226 raw shunt voltage  
Motor controller EN\_A readback  
Motor controller EN\_B readback  
Motor controller IN A  
Motor controller IN B  
Motor controller PWM duty %  
}  
}

Motor controller:

* PIN\_IN\_A \= 41, PIN\_IN\_B \= 35: motor direction inputs.  
* PIN\_EN\_A \= 40, PIN\_EN\_B \= 36: motor driver enable/diagnostic lines (read as inputs with pull-ups; can be actively driven LOW to disable).  
* PIN\_PWM \= 38: PWM output (20 kHz, adjustable duty).

External connections:

* I2C 1-3:  
  * Each has 3 sensors:  
    * BME688: Temperature, Humidity, Pressure, VOC values  
    * Si7021: Temperature, Humidity  
    * ZMOD4510: Temperature, Humidity, AQI, ozone, nitrogen dioxide  
  * Record each value along with which physical port it was connected to. When the device has an electronic serial number such as Si7021, read it and record it along with the measurement. 

* OneWire sensors:  
  * Auto detect sensors on all 4 buses. Record data to the JSON InfluxDB api. Include the port as well as the serial number. 

// somewhere else at least 250ms beforehand  
sensors.requestTemperatures();

String getTemperatureData() {  
  unsigned long nowMs \= millis();  
  String data \= "{";  
  data \+= "\\"measurement\\":\\"onewire\_temp\\",";  
  data \+= "\\"arduino\_millis\\":" \+ String(nowMs) \+ ",";

  bool firstSensor \= true;  
  // For each sensor, add its data to the JSON  
  for (uint8\_t i \= 0; i \< deviceCount; i++) {  
    DeviceAddress addr;  
    if (\!sensors.getAddress(addr, i)) {  
      // If we somehow lost the address, skip this index  
      continue;  
    }

    float tempC \= sensors.getTempC(addr);  
    if (tempC \== DEVICE\_DISCONNECTED\_C) {  
      Serial.print("Sensor ");  
      Serial.print(i);  
      Serial.println(" is disconnected\!");  
      continue;  
    }

    String addrTag \= addressToString(addr);

    // Add comma between sensors  
    if (\!firstSensor) {  
      data \+= ",";  
    }  
    firstSensor \= false;

    // Add this sensor's data  
    data \+= "\\"" \+ addrTag \+ "\\":" \+ String(tempC, 3);

    // Debug print  
    Serial.println("-----");  
    Serial.print("Sensor \#");  
    Serial.print(i);  
    Serial.print(" (");  
    Serial.print(addrTag);  
    Serial.print(") → ");  
    Serial.print(tempC, 3);  
    Serial.println(" °C");  
  }

  // Close the JSON structure  
  data \+= "}";  
  Serial.println("JSON → " \+ data);

String addressToString(const DeviceAddress deviceAddress) {  
  String s \= "";  
  for (uint8\_t i \= 0; i \< 8; i++) {  
    if (deviceAddress\[i\] \< 16\) s \+= "0";  
    s \+= String(deviceAddress\[i\], HEX);  
  }  
  s.toUpperCase();  
  return s;  
}  
