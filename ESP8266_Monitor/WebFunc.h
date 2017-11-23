
#ifndef WEBFUNC_H
#define WEBFUNC_H
#include <ESP8266WebServer.h>
#include "FiniteStateMachine.h"

extern ESP8266WebServer webServer;
extern struct StoreStruct storage;
extern FSM RemoteDevice;
extern bool connectToAP(String ssid, String pass);

extern State Standby;
extern State Checking;
extern State PowerDelay;
extern State On;
extern State Off;
extern State WifiError;
extern State OnDelay;
extern State Rebooting;
extern void saveConfig();

extern String FirmVer;
void systemProc();
void settingProc();
void handleRoot();
const char CONFIG_VERSION[4] = "RB8";

struct StoreStruct {

	char version[4];

	bool bWdtEn;
	uint32_t nWdtTime;
	char serverIP[32];

	char ssid[32];
	char pass[32];
	uint32_t lastPingTime;
	uint32_t reserveTime;

} storage;

void scanProc()
{
	int nCnt = WiFi.scanNetworks();
	// Build XML
	String xml = "";
	xml = "<?xml version='1.0'?>";
	xml += "<xml>";

	xml += "<Count>";
	xml += String(nCnt);
	xml += "</Count>";

	for (uint8_t idx = 0; idx < nCnt; idx++)
	{
		xml += "<AP";
		xml += String(idx);
		xml += ">";
		xml += WiFi.SSID(idx);
		xml += "</AP";
		xml += String(idx);
		xml += ">";

	}
	xml += "</xml>";
	webServer.send(200, "text/xml", xml);
}

void manualProc()
{

	if (RemoteDevice.isInState(Standby) || RemoteDevice.isInState(Checking))
	{
		RemoteDevice.transitionTo(Off);
		Serial.println("I have entered into manual rebooting status");
		webServer.send(200, "text/html", "successfully into manual reboot mode!");
	}
	else
	{
		webServer.send(200, "text/html", "in current status, we can't enter into reboot mode!");
	}
}

void connectProc()
{
	String ssid = webServer.arg("ssid");
	String pass = webServer.arg("pass");

	Serial.print(ssid);
	Serial.print(", ");
	Serial.println(pass);
	if (connectToAP(ssid, pass))
	{
		Serial.println("New Wifi setting is saved!");

		strcpy(storage.ssid, ssid.c_str());
		strcpy(storage.pass, pass.c_str());

		Serial.print("SSID is ");
		Serial.println(storage.ssid);
		Serial.print("Password is ");
		Serial.println(storage.pass);
		saveConfig();
		RemoteDevice.transitionTo(Standby);
	}
	else
	{
		Serial.println("Connecting to new AP is failed!");
		RemoteDevice.transitionTo(WifiError);
	}
	handleRoot();
}

void systemProc()
{
	String contents = "";
	contents += "<!DOCTYPE html>\
<html lang='en'>\
  <head>\
    <meta charset='utf-8'>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>ESP8266 Remote WDT Button Project Ver";
	contents += FirmVer;
	contents += "</title>\
    <style>body{padding-top:25px;padding-bottom:20px}.header {\
			border-bottom: 1px solid #E5E8E8;\
			margin-bottom: 0;\
			color: #D4AC0D\
		}\
      .jumbotron{text-align:center}.marketing{margin:40px 0}\
      .arduino h4{font-size:20px;color:#27AE60;margin-top:2px;padding-right:10px;padding-left:0; display:inline-block;}\
      .arduino h5{font-size:20px;color:#F1C40F;margin-top:2px;padding-right:0;padding-left:0px; display:inline-block;}\
      .arduino h6{font-size:16px;color:#27AE60;margin-top:2px;padding-right:0;padding-left:0px; display:inline-block;}\
      .clear{ clear:both;}\
      .align-center {text-align:center;}\
    </style>  \
  </head>\
  <body style='background-color:#EBDEF0'>\
  	<script>\
		function updateWdtSetting()\
		{\
			var xhttp = new XMLHttpRequest();\
			xhttp.onreadystatechange = function()\
			{\
				if (this.readyState == 4 && this.status == 200)\
				{\
					console.log(this.responseText);\
				}\
			};\
			var serverIP = document.getElementById('ipaddr');\
			var timeSetting = document.getElementById('wdttime');\
			var query = '/setting?serverip=';\
			query += serverIP.value;\
			query += '&wdttime=';\
			query += timeSetting.value;\
			console.log(query);\
			xhttp.open('GET', query, true);\
			xhttp.send();\
		}\
		function changeWdtMode(checkbox)\
		{\
			var xhttp = new XMLHttpRequest();\
			xhttp.onreadystatechange = function()\
			{\
				if (this.readyState == 4 && this.status == 200)\
				{\
					console.log(this.responseText);\
				}\
			};\
			var currSel = checkbox.checked;\
			console.log(currSel.toString());\
			xhttp.open('GET', '/wdt?enable=' + currSel.toString(), true);\
			xhttp.send();\
		}\
		function ScanSSID()\
		{\
			var xhttp = new XMLHttpRequest();\
			xhttp.onreadystatechange = function()\
			{\
				if (this.readyState == 4 && this.status == 200)\
				{\
					var select = document.getElementById('APList');\
					var x = select.length;\
					for (idx = 0; idx < x; idx++)\
						select.remove(0);\
					xmlDoc = this.responseXML;\
					xmlstr = xmlDoc.getElementsByTagName('Count')[0].firstChild.nodeValue;\
					var nCount = parseInt(xmlstr);\
					for (idx = 0; idx < nCount; idx++)\
					{\
						xmlmsg = xmlDoc.getElementsByTagName('AP' + idx)[0].firstChild.nodeValue;\
						var option = document.createElement('option');\
						option.text = xmlmsg;\
						option.value = xmlmsg;\
						select.add(option);\
					}\
					select.selectedIndex = '0';\
				}\
			};\
			xhttp.open('POST', '/scan', true);\
			xhttp.send();\
		}\
		ScanSSID();\
    </script>\
    <div class='container align-center'>\
      <div class='header'>\
        <h1>Welcome to ESP8266 Remote WDT System</h1>\
      </div>\
      <div class='row arduino'>\
        <div class='col-lg-12'>\
            <div class='clear'></div><br>\
            <div class='container'>\
                <div class='row arduino'>\
                    <div class='col-lg-12'>\
					<form action='/connect' method='POST'>\
                        <div class='clear'></div>\
                        <h4>HotSpot Setting</h4>\
                        <div class='clear'></div>\
                        <h4>Available SSID:</h4>\
                        <h5><select name='ssid' id = 'APList' style='max-width:150px;'>\
                        </select>\
                        </h5>&nbsp&nbsp<input type='button' onclick='ScanSSID()' value='Refresh'>&nbsp&nbsp\
                        <h4>Password:<input type='password' name='pass' id ='pass' maxlength='32' size='10'></h4>\
                        <h6><input type='submit' value='Connect'></h6>\
						<br	/>\
						<h4 style='width:140px;'>Local IP </h4><h5 id='LocalIP'>";

	if (WiFi.status() == WL_CONNECTED)
		contents += WiFi.localIP().toString();
	else
		contents += "not assigned";

	contents += "</h5><br / >\
					</form>\
                    </div>\
                </div>\
            </div>\
      </div>\
	  </div>\
		<div class='header'>\
		</div>\
		<div class='row arduino'>\
			<div class='col-lg-12'>\
				<div class='clear'></div>\
				<br>\
					<div class='container'>\
						<div class='row arduino'>\
							<div class='col-lg-12'>\
								<div class='clear'></div>\
								<br />";
	if (storage.bWdtEn)
		contents += "<h6><input id='wdten' type='checkbox' name='wdten' value='enable' checked='checked'";
	else
		contents += "<h6><input id='wdten' type='checkbox' name='wdten' value='enable'";
	contents += "onchange='changeWdtMode(this);'> Enable WDT Mode</h6>&nbsp&nbsp\
								</div>\
							</div>\
						</div>\
					</div>\
		<div class='header'>\
		</div>\
		<div class = 'container'>\
		<div class = 'row arduino'>\
		<div class = 'col-lg-12'>\
		<div class = 'clear'></div>\
		<br/>";
	contents += "<div class='clear'></div>\
								<h5 style='width:140px;'>Server IP </h5><h5><input id='ipaddr' type='text' name='serverip' required='required' value='";
	contents += String(storage.serverIP);
	contents += "' maxlength='15' size='9'></h5><br />\
								<h5 style='width:140px;'>Setting time </h5><input type='number' name='wdttime' id='wdttime' min='10' max='500' step='1' value='";
	contents += String(storage.nWdtTime);
	contents += "' size='9'><h4>(s)</h4><br />\
								<h6><input type='button' value='Update' onclick='updateWdtSetting()'></h6>\
							</div>\
						</div>\
					</div>\
				<div class='clear'></div>\
				<h4><a href='/'>Back to Home</a></h4><br />\
				<div class='header'>\
					<h1> Copyright OMG LLC 2017 </h1>\
				</div>\
			</div>\
		</div>\
	</div>\
  </body>  \
</html>";
	webServer.send(200, "text/html", contents);
}

void wdtProc()
{
	String response = "";
	if (webServer.hasArg("enable"))
	{
		String enableArg = webServer.arg("enable");
		if (enableArg == "true")
		{
			Serial.println("True");
			storage.bWdtEn = true;
		}
		else
		{
			Serial.println("False");
			storage.bWdtEn = false;
		}

		saveConfig();
		response = "OK!";
		RemoteDevice.transitionTo(Standby);
	}
	else
		response = "Invalid argumnet!";
	Serial.println(response);
	webServer.send(200, "text/html", response);
}

void handleRoot()
{
	String contents = "";
	contents += "<!DOCTYPE html>\
<html lang='en'>\
  <head>\
    <meta charset='utf-8'>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>ESP8266 Remote WDT Button Project Ver";
	contents += FirmVer;
	contents += "</title>\
    <style>body{padding-top:25px;padding-bottom:20px}.header {\
			border-bottom: 1px solid #E5E8E8;\
			margin-bottom: 0;\
			color: #D4AC0D\
		}\
      .jumbotron{text-align:center}.marketing{margin:40px 0}\
      .arduino h4{font-size:20px;color:#27AE60;margin-top:2px;padding-right:10px;padding-left:0; display:inline-block;}\
      .arduino h5{font-size:20px;color:#F1C40F;margin-top:2px;padding-right:0;padding-left:0px; display:inline-block;}\
      .arduino h6{font-size:16px;color:#27AE60;margin-top:2px;padding-right:0;padding-left:0px; display:inline-block;}\
      .clear{ clear:both;}\
      .align-center {text-align:center;}\
    </style>\
  </head>\
  <body style='background-color:#EBDEF0'>\
  	<script>\
		function Update()\
		{\
			var xhttp = new XMLHttpRequest();\
			xhttp.onreadystatechange = function () {\
				if (this.readyState == 4 && this.status == 200) {\
					xmlDoc = xhttp.responseXML;\
					var LocalIP = document.getElementById('LocalIP');\
					xmlstr = xmlDoc.getElementsByTagName('LocalIP')[0].childNodes[0].nodeValue;\
					LocalIP.innerText = xmlstr;\
					var DestIP = document.getElementById('DestIP');\
					xmlstr = xmlDoc.getElementsByTagName('DestIP')[0].childNodes[0].nodeValue;\
					DestIP.innerText = xmlstr;\
					var WdtEn = document.getElementById('WdtEn');\
					xmlstr = xmlDoc.getElementsByTagName('WdtEn')[0].childNodes[0].nodeValue;\
					WdtEn.innerText = xmlstr;\
					var TimeSetting = document.getElementById('TimeSetting');\
					xmlstr = xmlDoc.getElementsByTagName('TimeSetting')[0].childNodes[0].nodeValue;\
					TimeSetting.innerText = xmlstr;\
					var ReserveTime = document.getElementById('ReserveTime');\
					xmlstr = xmlDoc.getElementsByTagName('ReserveTime')[0].childNodes[0].nodeValue;\
					ReserveTime.innerText = xmlstr;\
					var Status = document.getElementById('Status');\
					xmlstr = xmlDoc.getElementsByTagName('Status')[0].childNodes[0].nodeValue;\
					Status.innerText = xmlstr;\
				}\
			};\
			xhttp.open('POST', '/status', true);\
			xhttp.send();\
			setTimeout('Update()', 2000);\
		}\
		Update();\
	</script>\
  	<div class='container align-center'>\
  		<div class='header'>\
  			<h1>Welcome to ESP8266 Remote WDT System</h1>\
  		</div>\
  		<div class='row arduino'>\
  			<div class='col-lg-12'>\
  				<div class='clear'></div>\
  				<br>\
  				<div class='container'>\
  					<div class='row arduino'>\
  						<div class='col-lg-12'>\
  							<div class='clear'></div>\
  							<h4>WDT <a href='/system'>System Setting</a></h4><br />\
						  	<h4>WDT function is </h4><h5 id='WdtEn'>";
	if (storage.bWdtEn)
		contents += "Enabled";
	else
		contents += "Disabled";
	contents += "</h5>&nbsp&nbsp&nbsp&nbsp<br />\
						  	<h4>Local IP</h4><h5 id='LocalIP'>";
	contents += WiFi.localIP().toString();
	contents += "</h5>&nbsp&nbsp&nbsp&nbsp\
  							<h4>Server IP</h4><h5 id = 'DestIP'>";
	contents += String(storage.serverIP);
	contents += "</h5>&nbsp&nbsp&nbsp&nbsp<br / >\
  							<h4>Setting WDT time is </h4><h5 id = 'TimeSetting'>";
	contents += String(storage.nWdtTime);
	contents += "</h5><h4> sec</h4>&nbsp&nbsp&nbsp&nbsp<br />\
  						</div>\
  					</div>\
  					<div class='clear'></div>\
  					<div class='header'>\
  					</div>\
  					<br />\
					<h4><a href='/update'>OTA Update</a></h4><br />\
					\
					<div class='clear'></div>\
  					<div class='header'>\
  					</div>\
  					<br />\
  					<h4>Current Status</h4><h5 id = 'Status'></h5> \
  					<br>\
  					<h4>Left time until WDT event </h4><h5 id = 'ReserveTime'>0</h5><h4> sec</h4>&nbsp&nbsp&nbsp&nbsp\
  					<div class='clear'></div>\
  					<div class='header'>\
  						<h1> Copyright OMG LLC 2017 </h1>\
  					</div>\
  				</div>\
  			</div>\
  		</div>\
  	</div>\
		</body>\
</html>";
	// Main Page
	webServer.send(200, "text/html", contents);
}

void settingProc()
{
	String response = "";
	
	if (webServer.hasArg("serverip") && webServer.hasArg("wdttime"))
	{
		String serverIP = webServer.arg("serverip");
		String wdtTime = webServer.arg("wdttime");
		strcpy(storage.serverIP, serverIP.c_str());
		
		//storage.nWdtTime = wdtTime.toInt();
		
		storage.nWdtTime = 300;
		saveConfig();
		response = "OK";
		RemoteDevice.immediateTransitionTo(Standby);
	}
	else
	{
		response = "Invalid argument";
	}

	webServer.send(200, "text/plain", response);
}

void statusProc()
{
	// Build XML
	String xml = "";
	xml = "<?xml version='1.0'?>";
	xml += "<xml>";

	xml += "<LocalIP>";
	xml += WiFi.localIP().toString();
	xml += "</LocalIP>";

	xml += "<DestIP>";
	if (String(storage.serverIP) == "")
		xml += "none";
	else
		xml += String(storage.serverIP);
	xml += "</DestIP>";

	xml += "<WdtEn>";

	if (storage.bWdtEn)
		xml += "enabled";
	else
		xml += "disabled";

	xml += "</WdtEn>";

	xml += "<TimeSetting>";
	xml += String(storage.nWdtTime);
	xml += "</TimeSetting>";

	xml += "<ReserveTime>";
	if (RemoteDevice.isInState(Standby))
		xml += "No";
	else if (RemoteDevice.isInState(Checking))
	{
		if ((millis() - storage.lastPingTime) <= storage.nWdtTime * 1000)
			xml += String(storage.nWdtTime - (millis() - storage.lastPingTime) / 1000);
		else
			xml += "0";
	}
	else
		xml += "No";
	xml += "</ReserveTime>";

	xml += "<Status>";
	String status;
	if (RemoteDevice.isInState(Standby))
		status = "standby";
	else if (RemoteDevice.isInState(Checking))
		status = "checking WDT";
	else if (RemoteDevice.isInState(PowerDelay))
		status = "delay power off";
	else if (RemoteDevice.isInState(OnDelay))
		status = "booting";
	else if (RemoteDevice.isInState(On))
		status = "Turning on";
	else if (RemoteDevice.isInState(Off))
		status = "Turning off";
	else if (RemoteDevice.isInState(WifiError))
		status = "IP not assinged";
	else if (RemoteDevice.isInState(Rebooting))
		status = "rebooting by webpage";

	xml += status;
	xml += "</Status>";

	xml += "<LastCheck>";
	xml += "</LastCheck>";

	xml += "</xml>";

	webServer.send(200, "text/xml", xml);
}

#endif // WEBFUNC_H
