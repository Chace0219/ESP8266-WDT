# ESP8266 based miners monitor and power management system

Target of this system is to monitor and reboot miners if they are in unreponsible status or high load status.

# Topology
- Multiple ESP8266 WDT monitor and controller (Mosquitto mqtt client)
- Mosquitto Mqtt server on Ubuntu server
- LAMP Backend and Frontend page to manage and configure wdt device and database
