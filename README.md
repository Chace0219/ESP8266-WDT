# ESP8266 based miners monitor and power management system

Target of this system is to monitor and reboot miners if they are in unreponsible status or high load status.

# Topology

- Multiple ESP8266 WDT monitor and controller (Mosquitto mqtt client)
- Mosquitto Mqtt server on Ubuntu server
- LAMP Backend and Frontend page to manage and configure wdt device and database

# Solution

- Using ping, ssh client fucntion and temperature sensors, relays
  
  If miner does not repond to ping request and ssh connection request for specified time, or temperature is too high by much load, 
  esp8266 controller will execute reboot or power on action to specified miner.
  
- Logging and configure on LAMP server

  Using Mqtt solution, devices will send logging message to server and server configure via publishing mqtt commands.

- Management and analyzing pages using LAMP

  
