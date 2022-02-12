# This fork adds the following features

(see corresponding branches with commits relative to master which tracks upstream/master)

## Support for using the ampel smoothly without having to frequently check the LED colors

* Triple short beep (unintrusive) that signals when ventilating can be finished. Criterion is that CO2 drops below 500 ppm (middle of blue band). A hysteresis is implemented to block further signaling until CO2 ppm has risen above 600 again.

* Long-short beep (unintrusive) that signals being close to the buzzer threshold. This signal is given once when CO2 reaches a margin of 100 ppm below the buzzer threshold. So you have time to start ventilation in time to prevent the buzzer really going loose. With hysteresis at 200 ppm below the buzzer threshold.

## Remote start of calibration

* The 2 min calibration procedure can be triggered via the web interface. The operation is secured by a simple passcode.

## Safe configuration of locally built firmwares

* There is an include statement for a .h-file which is not tracked in git. This can be used to, e.g., configure the wifi password without the risk of accidentally committing it.

## Minimal MQTT client code

* If an MQTT server name is configured this sends the plain CO2 value
  to a device dependent topic, that is automatically determined. There
  are no other configuration options. Configuration of the MQTT server
  name can be done via the web interface or via a #define statement in
  the code.

## Configure a buzzer threshold increase via the web interface

* It is often inconvenient when the buzzer suddenly starts. The
  original firmware (v >= 22) offers the possibility of switching the
  buzzer off completely via the "service menu". 
  
  As an alternative to that, in this firmware you have the less
  intrusive option of increasing the buzzer threshold by some
  amount. You can conveniently do this via the web interface. The
  increase is configured as a delta. So if you configure, say, 600
  ppm, and the original threshold is 1600 ppm, the buzzer will only
  sound starting at 2200 ppm.

## More intense blinking for higher ppm

* When you have disabled the buzzer or have extended the buzzer
  threshold there is no more feedback at which ppm level you are - the
  ampel will just quietly blink ahead. 
  
  This firmware adds a more intense blinking mode that starts at the
  original buzzer threshold, say 1600 ppm.

---------------------
Below is the original README.md
---------------------

# CO2-Ampel
The CO2-Ampel is a traffic light to show the carbon dioxide concentration in a room.

![CO2-Ampel](https://github.com/watterott/CO2-Ampel/raw/master/hardware/CO2-Ampel-Diffusor.jpg)

* [Microchip ATSAMD21](https://www.microchip.com/wwwproducts/en/ATSAMD21G18) ARM Cortex-M0+ Microcontroller
* Sensirion [SCD30](https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors/carbon-dioxide-sensors-scd30/) or [SCD40/SCD41](https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors/carbon-dioxide-sensor-scd4x/) CO2 Sensor
* Intelligent WS2812 RGB-LEDs
* Acoustic Buzzer
* Plus Version:
  * [Microchip ATWINC1500B](http://www.microchip.com/wwwproducts/en/ATWINC1500) WiFi/WLAN Network Controller
* Pro Version:
  * Air Pressure Sensor
  * [Microchip ATWINC1500B](http://www.microchip.com/wwwproducts/en/ATWINC1500) WiFi/WLAN Network Controller (optional)
  * [HopeRF RFM95W/RFM96W](https://www.hoperf.com/modules/lora/index.html) LoRa Transceiver (optional)
* USB-C connector for power and data/programming
* USB CDC+MSC Bootloader (Arduino compatible)
* Open-Source and released under the [Creative Commons Attribution Share-Alike License](https://creativecommons.org/licenses/by-sa/4.0/).
* **[Documentation on learn.watterott.com](https://learn.watterott.com/breakouts/co2-ampel/)**
