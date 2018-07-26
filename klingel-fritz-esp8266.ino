/*
 * Klingel an MQTT mit ESP8266
 * 
 * features:
 * http web page
 * mqtt
 * http OTA uploader
 *     To upload through terminal you can use: curl -F "image=@firmware.bin" esp8266-webupdate.local/firmware
 * 
 * support for 
 * D1 GPIO Klingel
 * 
 * 
 * 
 * GNU General Public License v2.0
 * 
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <stdio.h>
#include <string.h>

// Uses Ticker to do a nonblocking loop every 1 second to check status
#include <Ticker.h>

unsigned long lLastStatusUpdateSend = 0;
unsigned long lDeltaStatusUpdateSend = 2 * 60 * 1000;

// limit geklingelt melden einmal pro minute höchstens
unsigned long lLastChangeUpdateSend = 0;
unsigned long lDeltaChangeUpdateSend = 60 * 1000;

unsigned long lCountKlingel = 0;

const char* sVersion = "0.1.25";


#define CLOSED LOW
#define OPEN HIGH

#include <PubSubClient.h>
//Your MQTT Broker
const char* mqtt_server = "hwr-pi";

// MQTT client is also host name for WIFI
const char* mqtt_client_id = "klingel";


const char* mqtt_topic_cmd = "tuer/klingel/cmd";
const char* mqtt_topic_status = "tuer/klingel/status";
const char* mqtt_topic_version = "tuer/klingel/version";
const char* mqtt_topic_wifi = "tuer/klingel/wifi";
const char* mqtt_topic_zeit = "tuer/klingel/zeit";


//Your Wifi SSID
const char* ssid = "3A-peter@demus.de";
//Your Wifi Key
const char* password = "lothar2602lothar2602";

const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiClient espClient;
PubSubClient client(espClient);


int gpioLed = 2; // internal blue LED

int gpio_KlingelD1 = 5;  // D1


int FEHLER=0;
int AUS=1;
int AN=2;

const char* statusmeldung[] = { "Fehler", "Ruhe", "Klingel" };

int status = AUS;

bool SendUpdate = true;

Ticker ticker;

// milli sec
#define DEBOUNCETIME 120


//check gpio (input of PC871 opto koppler)
void statusTor() {
  int myOldStatus = status;
  int myNewStatus = FEHLER;
  static long buttonDownSince;
  static boolean buttonFired;
  
  if (digitalRead(gpio_KlingelD1) == OPEN  ) { myNewStatus = AUS; }
  if (digitalRead(gpio_KlingelD1) == CLOSED) { myNewStatus = AN; }


/*

// entprellen
  if ( myOldStatus != myNewStatus )  {
    
    if ( myNewStatus == AUS ) {
      buttonFired=false;
      buttonDownSince=0;
    }
    else if ( myNewStatus == AN && buttonDownSince==0)
    { // button is pressed but DEBOUNCETIME did not yet started counting
      buttonFired=false;
      buttonDownSince=millis();
    }

    if ( (myNewStatus==AUS) && ( (millis() - buttonDownSince) >= DEBOUNCETIME )  && (buttonFired==false) )
    {
      status=myNewStatus;
      buttonFired==true;
    }
  }

*/

  if ( myOldStatus != myNewStatus )
  {
    status=myNewStatus;
    Serial.print("dbg: Status has changed: ");
    Serial.println(statusmeldung[status]);
  }
}




void MqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = 0;

  Serial.println("mqtt call back");
  Serial.println( topic );
  Serial.println(  (const char *) payload );
  
  if ( strstr( (const char *)payload, "version") != NULL) {
    client.publish(mqtt_topic_version, sVersion);
    
    long lRssi = WiFi.RSSI();
    char sBuffer[50];
    sprintf(sBuffer, "RSSI = %d dBm", lRssi);
    client.publish(mqtt_topic_wifi, sBuffer);
    
    sprintf(sBuffer, "letztes Klingel vor %d min", (millis() - lLastChangeUpdateSend) / 60000 );
    client.publish(mqtt_topic_zeit, sBuffer);
  }
  if ( strstr( (const char *)payload, "status") != NULL) {
    statusTor();
    client.publish(mqtt_topic_status, statusmeldung[status]);
  }
}



void MqttReconnect() {
  while (!client.connected()) {
    Serial.print("Connect to MQTT Broker "); Serial.println( mqtt_server );
    delay(1000);
    if (client.connect(mqtt_client_id)) {
      Serial.print("connected: topic ");  Serial.println( mqtt_topic_cmd );
      client.subscribe(mqtt_topic_cmd);
    } else {
      Serial.print("failed: ");
      Serial.print(client.state());
      Serial.println(" try again...");
      delay(5000);
    }
  }
  Serial.println(" ok...");
}



void CheckDoorStatus() {
  int oldStatus = status;
  statusTor();
  if (oldStatus == status)
  {
    //new status is the same as the current status, return
    return;
  }
  else
  {
    Serial.print("Status has changed to: ");
    Serial.println(statusmeldung[status]);
    SendUpdate = true;
    digitalWrite(gpioLed, (status != AN) ); // LED on bei Klingel
  }
}


void setup(void) {

  Serial.begin(115200);
  delay(300);
  Serial.println("\n\nstarte Klingel MQTT Fritz!...");
  Serial.println();
  Serial.println(sVersion);
  Serial.println();

  pinMode(gpio_KlingelD1, INPUT_PULLUP);

  pinMode(gpioLed, OUTPUT);



//  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);


  while(WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


  MDNS.begin(mqtt_client_id);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);

  httpServer.begin();

  MDNS.addService("http", "tcp", 80);

  Serial.printf("HTTPUpdateServer ready! \n  Open http://%s.local%s in your browser and\n login with username '%s' and password '%s'\n", mqtt_client_id, update_path, update_username, update_password);
  Serial.printf("\nSketch version: %s\n", sVersion);


  client.setServer(mqtt_server, 1883);
  client.setCallback(MqttCallback);
  MqttReconnect();
  client.publish(mqtt_topic_version, sVersion);
  

  httpServer.on("/", []() {
    statusTor();

    if (status == AUS) {
      httpServer.send(200, "text/html", "Klingel ist aktuell AUS.<p>");
    }
    if (status == AN ) {
      httpServer.send(200, "text/html", "Klingel ist aktuell AN.<p>");
    }
    if (status == FEHLER ) {
      httpServer.send(200, "text/html", "Klingel ist aktuell FEHLER.<p>!!!</p>");
    }
 
    httpServer.send ( 302, "text/plain", "");
  });


  httpServer.on("/status", []() {
    long lRssi = WiFi.RSSI();
    char sBuffer[150];
    sprintf(sBuffer, "WiFi  RSSI = %d dBm\r\nVersion = %s\r\letztes Klingel vor %d min\r\nAnzahl Klingeln = %d\r\n", lRssi, sVersion, (millis() - lLastChangeUpdateSend) / 60000 , lCountKlingel);

    Serial.println( sBuffer ); // dbg

    httpServer.send(200, "text/plain", sBuffer);
 
    httpServer.send ( 302, "text/plain", "");
  });



  //Check the klingel status every 50 millisecond
  ticker.attach_ms( 50, CheckDoorStatus);

}



void loop(void) {
  httpServer.handleClient();

  if (!client.connected()) { MqttReconnect(); }
  client.loop();

  if (millis() - lLastStatusUpdateSend >  lDeltaStatusUpdateSend) {
    Serial.print("still running since ");
    Serial.print(millis()/lDeltaStatusUpdateSend);
    Serial.println(" minutes");
    lLastStatusUpdateSend = millis();
    SendUpdate = true;
  }

  if (SendUpdate)
  {
    Serial.print("mqtt status update: ");
    Serial.println(statusmeldung[status]);
    // not more than one message per minute?
    if ( millis() - lLastChangeUpdateSend > lDeltaChangeUpdateSend )
    {
        lLastChangeUpdateSend = millis();
        client.publish(mqtt_topic_status, statusmeldung[status]);
    } else {
        Serial.println("letztes Klingeln war gerade!");
    }
    lCountKlingel++; // check for debouncing succesful
    SendUpdate = false;
  }

}


