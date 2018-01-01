#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_IS31FL3731.h>

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 100, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);
long probes;
long connects;
String notFound;

Adafruit_IS31FL3731_Wing matrix = Adafruit_IS31FL3731_Wing();

String responseHTML = ""
  "<!DOCTYPE html PUBLIC \"-//WAPFORUM//DTD XHTML Mobile 1.2//EN\" \"http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd\">"
  "<html><head><title>Shadow WiFi Access Point</title></head><body>"
  "<h1>Shadow WiFi Access Point</h1>"
  "<h2>Probe requests: {{probes}}</h2>"
  "<p>This is not your standard WiFi access point.  It is a <i>captive portal</i> based on an ESP8266 chip.</p>"
  "<p>This particular project is focused on getting to know the ESP8266 better, and understanding the implications of cheap WiFi microcontroller</p>"
  "<p>The scrolling display on the access point shows the number of probe requests since the device was turned on.  "
  "Probe requests are how mobile WiFi devices discover nearby access points.</p> "
  "<p>They are interesting because each probe request includes the device's MAC address, a value that is globally unique to the device.  "
  "This means that any access points can passively determine when any WiFi device is in range.  All you need to do is walk by with your phone on.</p>"
  "</p>The privacy implications are interesting, if not yet disturbing.</p>"
  "<p>The device also displays the number of actual connections to the access point, and the last URI that the user tried, and failed, to reach via this access point.</p>"
  "</body></html>";

class Scroller
{
  // Class Member Variables
  String _text;
  String _nextText;
  int _interval;
  unsigned long _previousMillis;
  uint _length;
  uint _counter;
  void (*_scrollComplete)();

  public:
  Scroller(String text, int interval, int brightness) {
    if (!matrix.begin()){
      Serial.println("Unable to find wing.");
    }
    matrix.setRotation(2);
    matrix.setTextSize(1);
    matrix.setTextWrap(false);  // we dont want text to wrap so it scrolls nicely
    matrix.setTextColor(brightness);

    setText(text);
    _interval = interval;
    _previousMillis = 0;
    _counter = 0;
  }

  void setText(String text){
    _text = "   " + text;
    _length = -(_text.length() * 6);
  }

  void setNextText(String nextText){
    _nextText = nextText;
  }

  void setScrollCompleteCallback(void (*scrollComplete)()){
    _scrollComplete = scrollComplete;
  }

  void Update()
  {
    // check to see if it's time to change the state of the LED
    unsigned long currentMillis = millis();

    if (currentMillis - _previousMillis >= _interval){
      _previousMillis = currentMillis;
      
      matrix.clear();
      matrix.setCursor(_counter, 0);
      matrix.print(_text);
      
      _counter--;
      if (_counter == _length){
        _counter = 0;
        _scrollComplete();
        if (_nextText.length() > 0){
          setText(_nextText);
          _nextText = "";
        }
      }
    }
  }
};

Scroller scroller("Starting...", 50, 10);

void scrollComplete(){
  Serial.println("Scroll complete!");
  scroller.setNextText("Shadow WiFi AP - probes: " + String(probes) + " - clients: " + String(connects) + " - last URI: " + notFound);
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WIFI_EVENT_STAMODE_CONNECTED");
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WiFi lost connection");
      break;
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WIFI_EVENT_STAMODE_AUTHMODE_CHANGE");
      break;
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.print(millis());
      Serial.print(" => ");
      Serial.println("WIFI_EVENT_STAMODE_GOT_IP");
      Serial.println(WiFi.localIP());
      break;
    case WIFI_EVENT_STAMODE_DHCP_TIMEOUT:
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WIFI_EVENT_STAMODE_DHCP_TIMEOUT");
      break;
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      connects++;
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WIFI_EVENT_SOFTAPMODE_STACONNECTED");
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WIFI_EVENT_SOFTAPMODE_STADISCONNECTED");
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      probes++;
//      Serial.print(millis());
//      Serial.print(" => ");
//
//      Serial.println("WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED");
      break;
    case WIFI_EVENT_MAX:
      Serial.print(millis());
      Serial.print(" => ");

      Serial.println("WIFI_EVENT_MAX");
      break;
  }
}

void setup() {
  scroller.setScrollCompleteCallback(scrollComplete);
  
  Serial.begin(9600);
  Serial.println("Captive portal test");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.onEvent(WiFiEvent);
  WiFi.softAP("Shadow WiFi AP");

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // replay to all requests with same HTML
  webServer.on("/generate_204", handleGenerate204);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

void handleGenerate204(){
  Serial.println("handleGenerate204()");
  webServer.send(204, "text/html", "");
}

void handleNotFound(){
  String notFoundUri = webServer.uri();
  Serial.println("handleNotFound(): " + notFoundUri);
  if (notFoundUri != "/favicon.ico" && notFoundUri.length() > 4){
    notFound = notFoundUri;
  }
  String newResponse = responseHTML;
  newResponse.replace("{{probes}}", String(probes));
  webServer.send(200, "text/html", newResponse);
}

void loop() { 
  scroller.Update();
  dnsServer.processNextRequest();
  webServer.handleClient();
}

