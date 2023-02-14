# device

I will build the controller with an Adafruit Huzzah32. The controller will initially control our Hayward variable speed filter pump and Hayward vacuum booster pump (power only). The variable speed pump provides a 12v power supply. 

I plan to add the Hayward heater, filter pressure, and temp/humidity/pressure sensors.

Limited state data will persist in onboard SPIFFS. Primary control and data persistence will be via AWS IoT.