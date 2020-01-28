########################################################################################
# (c) 2019 - 2020 Markus Schultheis
# This Code is under GPL3.0
########################################################################################


#include <WebSocketsServer.h>
#include <WebSocketsClient.h>
#include <WebSockets.h>
#include <SocketIOclient.h>
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
//#include <splash.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <SPI.h>
#include <Wire.h>

// Entfernung messen
long dauer=0; 
long entfernung=0; 
long copy_entfernung=0;

uint8_t remoteIPAddress = 0;

boolean authenticated = false;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

//Variable for first run that display server information
boolean firstRun = true;

// Auxiliar variables to store the current output state
String output_relay_state = "off";
bool RELAYstatus = LOW;
const int output_RELAY = 4;
const int button1 = 2;

// Settings for connected HC-SR04 device
const int trigger_PIN = 5 ; 
const int echo_PIN = 16; 

//WLAN Counter for reconnect
int reconnected = 0;
boolean opened = false;

// WIFI settings
const char* ssid     = "your_SSID";
const char* password = "your_Password";

//Webserver
String authFailResponse = "Authentication has failed!";
char* www_username = "aUsername";
char* www_password = "aPassword";
const char* www_realm = "ESP32 Garage Authentification";

//Websocket Server
WebSocketsServer webSocket = WebSocketsServer(8000); 

// web server settings
WebServer server(80);

//Setup routine at begin of Program or boot up sequence
void setup() {
    
    Serial.begin(115200);

    //make settings for display. Showing informations during setup phase
    setup_display();

    // Initialize the output variables as outputs
    // Set RELAY to output and Inputbutton
    pinMode(output_RELAY, OUTPUT);
    digitalWrite(output_RELAY, LOW);
    pinMode(button1, INPUT);

    // Settings for Sensor
    pinMode(trigger_PIN, OUTPUT);
    pinMode(echo_PIN, INPUT);

    // Connect to Wi-Fi network with SSID and password
    Serial.print("Connecting to ");
    Serial.println(ssid);
    display.clearDisplay();
    print_display("\nconnecting to\n" + String(ssid));
    connect_WLAN();

    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    //display that WiFi is connected
    display.clearDisplay();
    print_display("WiFi connected!\n");
    print_display(WiFi.localIP(),0,16);
    delay(4000);

    server.on("/", HTTP_GET, []() {
        if (!server.authenticate(www_username, www_password)) {
            authenticated = false;
            return server.requestAuthentication(BASIC_AUTH, www_realm, authFailResponse);
        } 
        authenticated = true;
        display.clearDisplay();
        print_display("Authentication succesfull");
        RELAYstatus = LOW;
        Serial.println("RELAY Status: OFF");
        server.send(200, "text/html", SendHTML(RELAYstatus));
        server.on("/relay", handle_relay);
        server.on("/favicon.ico", send_icon);
    });

    //listening for incoming connections
    server.begin();
    webSocket.begin();
    Serial.println("WebSocket listening on Port 81");
    webSocket.onEvent(webSocketEvent);
}

void loop() {

  digitalWrite(trigger_PIN, LOW); 
  delay(5); 
  digitalWrite(trigger_PIN, HIGH); 
  delay(10);
  digitalWrite(trigger_PIN, LOW);
    
    if (firstRun) {
        display.clearDisplay();
        print_display("Webserver is Ready \n");
        print_display(WiFi.localIP(),0,8*2);
        firstRun = false;
        delay(2000);
        display.setCursor(0,0);
    }

    check_input();
    if (WiFi.status() != WL_CONNECTED) {
      connect_WLAN();
    };
    
    server.handleClient();
    display.clearDisplay();
    print_display("Abstand: " + String(get_Entfernung()) + " cm", 0, 50);

    webSocket.loop();                           // constantly check for websocket events
    
}

void setup_display() {
    Serial.println("Setting Display.....");

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    Serial.println("display.begin()....");

    display.display();
    delay(1000);

    // Clear the buffer.
    display.clearDisplay();

    // text display tests
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
}

void print_display(String text) {
    display.setCursor(0,0);
    display.println(text);
    display.display();
    return;
}

void print_display(String text, int posX, int posY) {
    display.setCursor(posX,posY);
    display.print(text);
    display.display();
    return;
}

void print_display(IPAddress ip, int posX, int posY) {
    display.setCursor(posX,posY);
    display.print(ip);
    display.display();
    return;
}

void check_input() {
    if (digitalRead(button1) == 1) {
        delay(20);
        print_display("Button pressed",0,5);
        Serial.println("Button pressed");
        pressed_button();
    }
}



void handle_OnConnect() {
    RELAYstatus = LOW;
    Serial.println("RELAY Status: OFF");
    server.send(200, "text/html", SendHTML(RELAYstatus));
}

void handle_relay() {
    if (authenticated) {
        display.clearDisplay();
        print_display("received relay toggle",0,0);
        digitalWrite(output_RELAY, HIGH);
        Serial.println("Relay: ON");
        delay(1000);
        digitalWrite(output_RELAY, LOW);
        Serial.println("Relay: OFF");
        server.send(200, "text/html", SendHTML(true));
    } else {
        display.clearDisplay();
        print_display("Not authenticated");
        server.send(404, "text/html", "<h1>Not Authenticated</h1>");
    }
}

void send_icon() {
    server.send(200, "image/x-icon;base64", "iVBORw0KGgoAAAANSUhEUgAAABoAAAAaCAYAAACpSkzOAAAABmJLR0QA/wD/AP+gvaeTAAABnklEQVRIibXVQUhUURTG8d9oolYWUpuwAjGIoBCClhFBu2jlUoVaS/tW7VyIi5auol1BRJuCCAKJFAmDXAmtSmunBJGCWPpc3DvM9JyZ3szc+eDCe5fzzv+dc885l9Z1HrMYxVHcacNXXV3ED2T4jQ/xeQ49qSCDWI+Oa61X6E8BetoAkmEft9qFnIuOGoFWJIioFx//A8rwSSiWtjSAdwVgW3iIY61ATuOEENnLArAMG5hqBtKLz1jFsFDGr+s4f4zreBvffzUDulvlaB0X0If5HOQJuuM3MyolX1hvcg6/4qyQzm9xbx6laH8bf7GHa0Uhlxwu632VsXMVu/iDy5jATrR7UBRSUvsspnN2j+L+zyqbmaIQuF8D8gJdObtBocIybGKyGciYSgrKa1H9zr+CcZwsCjgipGYvB/mCU838aSOdwXuH07UizLokGsFaDcgzHE8FGcL3HGAL91IBCONlKQdZFfonqWZzkGUJD72sUaGjy5BtYWgm13P/RjPXCUiXUGnVWugEiDCBl1UiutkpECGyG8KdU2ps2poOAEClrVorZujyAAAAAElFTkSuQmCC>");
}

void handle_NotFound(){
    server.send(404, "text/plain", "<h1>no way here</h1>");
}

String SendHTML(uint8_t relay){
    String ptr = "<!DOCTYPE html> <html lang=\"de\">\n";
    ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    ptr +="<title>Garagentor Steuerung V1.3 (c) 2020</title>\n";
    ptr +="<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\" integrity=\"sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T\" crossorigin=\"anonymous\">\n";
    ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
    ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
    ptr +=".button-on {background-color: #3498db;}\n";
    ptr +=".button-on:active {background-color: #2980b9;}\n";
    ptr +=".button-off {background-color: #34495e;}\n";
    ptr +=".button-off:active {background-color: #2c3e50;}\n";
    ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
    ptr +="</style>\n";
    ptr +="<script type=\"text/javascript\">\n const socket = new WebSocket('ws://" + WiFi.localIP().toString() + ":8000');\n";
    ptr +="socket.onopen = (event) => {\n //on connection do smth\n socket.send('Test');\n }\n socket.onmessage = (event) => {\n var distance = document.getElementById('distance');\n //message from server\n console.log(event.data);\n distance.innerHTML=(\"Garagentor&oumlffnung: \" + event.data + \" cm\");\n}\n</script>\n";
    ptr +="</head>\n";
    ptr +="<body><div class=\"container\">\n";
    ptr +="<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js\"></script>\n";
    ptr +="<script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.7/umd/popper.min.js\" integrity=\"sha384-UO2eT0CpHqdSJQ6hJty5KVphtPhzWj9WO1clHTMGa3JDZwrnQq4sF86dIHNDz0W1\" crossorigin=\"anonymous\"></script>\n";
    ptr +="<script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/js/bootstrap.min.js\" integrity=\"sha384-JjSmVgyd0p3pXB1rRibZUAYoIIy6OrQ6VrjIEaFf/nJGzIxFDsf4x0xIM+B07jRM\" crossorigin=\"anonymous\"></script>\n";
    ptr +="<div class=\"mx-auto\" style=\"with: 200px;\"><nav class=\"navbar navbar-light bg-light\"><a class=\"navbar-brand\" href=\"#\"><h1>Garagensteuerung</h1></a></nav></header></div><hr class=\"my-4\">\n";
  
    if (opened) {
      ptr +="<div class=\"alert alert-danger\" role=\"alert\"><center><p>Garage schlie&szligen</p></center><a class=\"button button-switch\" href=\"/relay\">zu</a></div>\n";
    }
    else {
      ptr +="<div class=\"alert alert-success\" role=\"alert\"><center><p>Garage &oumlffnen</p></center><a class=\"button button-switch\" href=\"/relay\">auf</a></div>\n";
    }
    
    ptr += "<span id=\"distance\" class=\"badge badge-primary\">" + String(entfernung) + " cm</span>\n";
    ptr +="</div></body>\n";
    ptr +="</html>\n";
    return ptr;
}

void pressed_button() {
    display.clearDisplay();
    print_display("received relay toggle from Button",0,0);
    digitalWrite(output_RELAY, HIGH);
    Serial.println("Relay: ON --> BUTTON PRESSED");
    delay(1000);
    digitalWrite(output_RELAY, LOW);
    Serial.println("Relay: OFF");
}

void connect_WLAN(){
    if (reconnected > 100) {
      reconnected = 0; 
    } else {
      reconnected++;
    }
    
    print_display("WLAN Counter: " + String(reconnected), 0, 50);
    Serial.print("Connect count" + String(reconnected));
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
}

void webSocketEvent(uint8_t num, 
                    WStype_t type, 
                    uint8_t * payload, 
                    size_t length) { // When a WebSocket message is received
  
  switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                print_display("Connect: " + num, 0, 0);
                // send message to client
                webSocket.sendTXT(num, "This is a message from the server");
                webSocket.sendTXT(num, "document.getElementById(\"distance\").innerHTML=\"" + String(get_Entfernung()) + " in cm\";\n");
                webSocket.sendTXT(num, "document.createElement(\"P\").innerHTML=\"New Client Connected!\";\n");
            }
            remoteIPAddress = num;
            break;
       }
}

long get_Entfernung() {
// Wenn kleiner als 15 cm dann ist die Garage offen
  dauer = pulseIn(echo_PIN, HIGH); 
  entfernung = (dauer/2) * 0.03432;
  copy_entfernung = entfernung;

  String distance = String(entfernung);
  webSocket.sendTXT(remoteIPAddress, distance);
  
  if (copy_entfernung!=entfernung){
    
    Serial.println(copy_entfernung + ", " + entfernung);
    webSocket.sendTXT(remoteIPAddress, "Abstand: " + distance + " in cm");
    if (entfernung >= 15) {
      opened = false;   
    } else {
      opened = true;
    }  
  }  

return entfernung;
}
