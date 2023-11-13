# water_heater_timer
A simple water Heater Arduino Sketch That Automatically gets time/date and controls a water heater to avoid paying for high power costs during peak hours.

# Hardware Requirements
This is designed for use on a Heltec ESP32 Wifi Kit v3 board using an Arduino IDE. Additionally, a 220V SPDT Relay with Opto-isolation is required to switch the Heater on/off. 

I would suggest the NOYITO 30A 2-Channel Relay Module High Low Level Trigger with Optocoupler Isolation Load DC 30V AC 250V 30A for PLC Automation Equipment Control, Industrial Control (2-Channel 5V).

# Things You Need to Adjust in the Sketch
* Line 8 & 9 adjust your WiFi Settings
* Line 13 : "-5" is where you need to set your timezone. US Eastern is GMT "-5", yours will be different 
* Line 16 Adjust for your heater pin connection - since most US households are split phase you will need to connect both relays together to turn both hot wires on at the same time or add an additional heaterPin output
* Line 97-115 - this is your timer logic. You will need to adjust this to meet your requirements. Mine was weekdays winter 6am-10am heater OFF and Summer 1-8PM heater OFF. 
