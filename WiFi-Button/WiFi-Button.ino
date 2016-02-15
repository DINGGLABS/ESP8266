/** ===========================================================
 * \Arduino 1.6.6_ESP8266
 * \Board         Olimex 
 * \CPU Frequency 80MHz
 * \Upload Speed  230400
 *
 * @{
 ============================================================== */

/* DOCUMENTATION:
 *  To add a new api client request:
 *  1. Add an API URI define ("/api/...")
 *  2. Add a server.on(...) in setupPossibleClientRequests()
 *  3. Add defined handler function
 *  4. Program handler function
 *  
 *  To add a new static client request:
 *  1. Add a static URI define ("/...")
 *  2. Add a path define ("/data/...")
 *  3. Add a server.serveStatic(...) in setupPossibleClientRequests()
 *  
 *  ToDo's:
 *  - program a proper ApiWebClient with defined requests
 *  - test all requests extensive :)
 *  - add more functionalities
 */

/* Configurable defines --------------------------------------- */
#define FIRMWARE_VERSION            (1.00)

/* system behavior defines */
#define DELAY_UNTIL_SM_LOOP_ESCAPE    10000     // in ms
#define DELAY_UNTIL_AP_LOOP_ESCAPE     1000     // in ms
#define LED_FEEDBACK_DURATION          3000     // in ms
#define MAX_DURATION_SHORTCLICK        1000     // max. duration in ms a SHORTCLICK will be recognized
#define MIN_DURATION_ULTRALONGCLICK    5000     // min. duration in ms an ULTRALONGCLICK will be recognized
#define DELAY_PER_NTP_PACKAGE          1000     // in ms
#define STD_DELAY                        20     // in ms

/* LED defines */
#define LED_FREQUENCY                  2000     // in Hz
#define CLICK_LED_BRIGHTNESS_DIVISOR      4     // divisor of the LED brightness during a button click (to reduce current consumption)

/* piezo defines */
#define PIEZO_FREQUENCY                3600     // in Hz
#define STD_PIEZO_FREQUENCY_DIVISOR       2     // f_piezo = PIEZO_FREQUENCY / value) -> (the higher the value the lower the tone)
#define HIGH_PIEZO_FREQUENCY_DIVISOR      1     // divisor for a high tone
#define LOW_PIEZO_FREQUENCY_DIVISOR       4     // divisor for a low tone

/* EEPROM defines */
#define MAX_SIZE_EEPROM (MAX_SIZE_SSID + MAX_SIZE_SSID_PW)   // max EEPROM size to allocate
/* EEPROM memory map (4... 4096 bytes)
 *  config:
 *  0     ssid                    // 0... MAX_SIZE_SSID - 1
 *  32    ssid_pw                 // MAX_SIZE_SSID... MAX_SIZE_SSID_PW - 1
 *  95    ...
 */

/* wifi defines */
#define HTTP_PORT                      80
#define UDP_PORT                     2390

#define MDNS_NAME                "wifi-button"  // http://wifi-button.local/
#define MAX_SIZE_SSID                    32     // WL_SSID_MAX_LENGTH
#define MAX_SIZE_SSID_PW                 63     // L_WPA_KEY_MAX_LENGTH

#define JSON_DATA_BUFFER_SIZE   (JSON_OBJECT_SIZE(1))   // = 140 bytes

/* http status codes */
#define SUCCESS         200     // OK
#define BAD_REQUEST     400     // Bad Request
#define NOT_FOUND       404     // Not Found
#define NOT_ACCEPTABLE  406     // Not Acceptable
#define TOO_LARGE       413     // Request Entity Too Large

/* API URIs */
#define URI_API_GPIO_LEDS       ("/api/gpio/leds")
#define URI_API_CONFIG_SSID     ("/api/config/ssid")
#define URI_API_CONFIG_RESET    ("/api/config/reset")
#define URI_API_TIME            ("/api/time")
#define URI_API_UPLOAD_FIRMWARE ("/api/upload/firmware")
#define URI_API_UPLOAD_PATH     ("/api/upload/path")
#define URI_API_UPLOAD_FILE     ("/api/upload/file")
#define URI_API_UPLOAD_PICTURE  ("/api/upload/picture")

/* static URIs */
#define URI_INDEX_HTML      ("/")
#define URI_AWC_SIDE_HTML   ("/awc-side-nav.html")
#define URI_WELCOME_HTML    ("/welcome.html")
#define URI_SAMPLE_HTML     ("/sample.html")
#define URI_SETTINGS_HTML   ("/settings.html")
#define URI_STYLES_CSS      ("/css/styles.css")
#define URI_APP_JS          ("/js/app.js")

/* SPIFFS path defines */
#define PATH_SRV_FILES      ("/srv")  // "serve"-files 
#define PATH_INDEX_HTML     ("/srv/index.html")
#define PATH_AWC_SIDE_HTML  ("/srv/awc-side-nav.html")
#define PATH_WELCOME_HTML   ("/srv/welcome.html")
#define PATH_SAMPLE_HTML    ("/srv/sample.html")
#define PATH_SETTINGS_HTML  ("/srv/settings.html")
#define PATH_STYLES_CSS     ("/srv/css/styles.css")
#define PATH_APP_JS         ("/srv/js/app.js")

/* Includes --------------------------------------------------- */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>  // to replace local ip with e.g. "esp8266_id.local" ('Bonjour' on Windows or 'Avahi' on Linux have to be installed)
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "FS.h"

#include "myHTML.h"

#ifdef ESP8266
extern "C"
{
  #include "user_interface.h"
}
#endif

/* PIN definitions -------------------------------------------- */
#define BUTTON               4
#define LED_RED             13
#define LED_GREEN           14
#define LED_BLUE            12
#define PIEZO                5

#define WAKEUP_EN           15
#define GPIO0                0
#define GPIO2                2

/* General defines -------------------------------------------- */
#define FOREVER     true

/* duty cycles */
#define DC_100              51  // 255/5 = 51
#define DC_50       (DC_100/2)
#define DC_25       (DC_100/4)
#define DC_5       (DC_100/20)

/* colors */
#define RED                       255, 0, 0
#define GREEN                     0, 255, 0
#define BLUE                      0, 0, 255
#define YELLOW                  130, 130, 0
#define TURQUOISE               0, 150, 110
#define VIOLETT                 130, 0, 130

/* color allocations */
#define COLOR_SHORT_BUTTON_CLICK        RED
#define COLOR_LONG_BUTTON_CLICK       GREEN
#define COLOR_ULONG_BUTTON_CLICK       BLUE

/* Global variables ------------------------------------------- */
float currentFirmwareVersion = FIRMWARE_VERSION;

/* events */
enum events
{
  EVENT_NONE = 0,
  EVENT_SHORTCLICK,
  EVENT_LONGCLICK,
  EVENT_ULTRALONGCLICK
};
byte event = EVENT_NONE;

/* timing reference variables */
unsigned long startMillis = millis();      // timing reference in ms
unsigned long startMicros = micros();      // timing reference in us

/* SSID data */
const char DEFAULT_SSID[MAX_SIZE_SSID] = "ESP8266-AP (192.168.4.1)"; // do not use '_' or '@' (issue #661)
const char DEFAULT_SSID_PW[MAX_SIZE_SSID_PW] = {0};

char ssid[MAX_SIZE_SSID] = {0};
char ssid_pw[MAX_SIZE_SSID_PW] = {0};

/* TCP server at port 80 will respond to HTTP requests */
ESP8266WebServer server(HTTP_PORT);

/* mDNS responder */
MDNSResponder mdns;

/* FS */
String pathSaved = PATH_SRV_FILES;
String pathSavedFile = URI_INDEX_HTML;

/* LED variables */
volatile unsigned int phaseInterruptCounterLED = 0;
struct rgbLEDs
{
  unsigned int r;  // red 0... 255
  unsigned int g;  // green 0... 255
  unsigned int b;  // blue 0... 255
}led;

/* piezo variables */
volatile unsigned int tickerCounterPiezo = 0;
boolean piezoEnable = true;
boolean piezoOn = false;
unsigned int piezoFrequencyDivisor = STD_PIEZO_FREQUENCY_DIVISOR;

/* time structure */
struct times
{
  int hr = 0;
  int min = 0;
  int sec = 0;
  String timeString;
  boolean sumTime = false;  //standard (false) or summer time (true)
  float UTC = 1;
}myTime;

/* Network Time Protocol (NTP) time server ip and server name */
IPAddress timeServerIP;
const char ntpServerName[] = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;     // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
WiFiUDP udp;                        // an UDP instance to let us send and receive packets over UDP

/** ===========================================================
 * \fn      setup
 * \brief   standard Arduino setup-function with all inits
 *
 * \param   -
 * \return  -
 ============================================================== */
void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("");
  Serial.println("start setup");
  Serial.println("");
  Serial.printf("Sketch size = %u\n", ESP.getSketchSize());
  Serial.printf("Free size = %u\n", ESP.getFreeSketchSpace());
  initGPIOs();
  getSSIDDataFromEEPROM();
  setupPossibleClientRequests();
  setStationMode();
  setupWiFi();
  //setSleepType(); // shows only effect in station mode!
  setupFS();
//  deleteIndexHTML(); // debugging
//  formatFS(); // debugging
  setupIndexHTML();
  setupTimer1();  
  Serial.println("start loop");
  Serial.println("");
}

void initGPIOs()
{
  Serial.println("init GPIOs");
  Serial.println("");

  pinMode(WAKEUP_EN, OUTPUT);     // HIGH = reset disabled, LOW = reset enabled
  digitalWrite(WAKEUP_EN, HIGH);  // low active -> uses 1.5mA (3.3V/2k2) as long as devie is on
  
  pinMode(BUTTON, INPUT);
  //detachInterrupt(BUTTON);
  
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, HIGH);    // low active!
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, HIGH);  // low active!
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, HIGH);   // low active!
  pinMode(PIEZO, OUTPUT);

  pinMode(GPIO0, INPUT);

  pinMode(GPIO2, OUTPUT);
  digitalWrite(GPIO2, HIGH);
  
  resetAllOutputs();
}

void getSSIDDataFromEEPROM()
{
  /* read data from the EEPROM */
  Serial.println("read SSID data from the EEPROM");
  
  /* get saved data from the EEPROM */
  EEPROM.begin(MAX_SIZE_EEPROM);

  /* read SSID */
  for (byte i = 0; i < MAX_SIZE_SSID; i++) ssid[i] = EEPROM.read(i);
  Serial.print("SSID = "); Serial.println(ssid);
  
  /* read SSID_PW */
  for (byte i = 0; i < MAX_SIZE_SSID_PW; i++) ssid_pw[i] = EEPROM.read(MAX_SIZE_SSID + i);
  Serial.print("SSID_PW = "); Serial.println(ssid_pw);
  Serial.println("");

//  /* clear EEPROM */
//  for (byte i = 0; i < MAX_SIZE_EEPROM; i++) EEPROM.write(i, 0);
  
  /* release the RAM copy of the EEPROM contents */
  EEPROM.end();   // .end includes a .commit();
}

void setStationMode()
{
  printWifiMode();
  Serial.println("set wifi mode");
  WiFi.mode(WIFI_STA);
  printWifiMode();
  Serial.println("");
}

void setSleepType()
{
  printSleepType();
  Serial.println("set sleep type");
  //wifi_set_sleep_type(MODEM_SLEEP_T);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  printSleepType();
  Serial.println("");
}

void setupFS()
{
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system!");
    while(1);
  }
  Serial.println("success");
  Serial.println("");
} 

void deleteIndexHTML()
{
  Serial.println("delete index.html (debugging)");
  File f = SPIFFS.open(PATH_INDEX_HTML, "w"); // "w" instantly overwrites the file if existent (so size is 0!)
  f.close();  
}

void formatFS()
{
  Serial.println("Formate FS...");
  if (!SPIFFS.format())
  {
    Serial.println("Failed to format file system!");
    return;
  }
  Serial.println("success");
  
  showFiles("/"); // FS should be empty now
}

void setupIndexHTML()
{
  Serial.println("setup index.html");
  
  /* get size of index.html */
  int indexHTMLSize = getFileSize(PATH_INDEX_HTML);
  Serial.print("indexHTMLSize = ");  Serial.println(indexHTMLSize);
  Serial.println("");

  /* create index.html if non existent or empty */
  if ((indexHTMLSize == -1) || (indexHTMLSize == 0))
  {
    Serial.println("index.html does not exists or is empty");
    Serial.println("try creating index.html...");

    /* get myHTML code */
    String indexHtmlString =  //blup string size ist begrenzt!
        HTML_head
      + HTML_setLEDsForm
      + HTML_setSSIDForm
      + HTML_setTimeForm
//      + HTML_uploadFirmwareForm
//      + HTML_saveFileForm
//      + HTML_resetDeviceForm
      + JS_functions
      + HTML_tail;

    Serial.print("indexHtmlString.length() = ");  Serial.println(indexHtmlString.length());

    /* check size */
    Serial.println("check if string is too large to save");
    size_t stringSize = indexHtmlString.length();
    if (stringSize > 1024*1000) //blup MAX_SIZE_HTML < 1Mbyte
    {
      Serial.println("string is too large!");
      while(1); //return false;
    }

    /* show string */
    Serial.println("show indexHtmlString:"); //blup
    Serial.println(indexHtmlString); //blup

    /* open index.html for writing */
    Serial.println("open index.html for writing");
    File indexHTML = SPIFFS.open(PATH_INDEX_HTML, "w");

    /* save string into the file */
    Serial.println("save string into index.html");
    indexHTML.print(indexHtmlString);
    indexHTML.close();
        
    /* control */
    Serial.println("control index.html...");
    File indexHTML_ = SPIFFS.open(PATH_INDEX_HTML, "r");
    if (!indexHTML_){Serial.println("Failed to open indexHTML_ file for reading!"); while(1);}
    Serial.print("indexHTML_.size() = ");  Serial.println(indexHTML_.size());
    if (indexHTML_.size() != stringSize) {Serial.println("size of index.html is unequal string size!"); while(1);}
    Serial.println(indexHTML_.readString());
    Serial.println("success");
    Serial.println("");
    indexHTML_.close();
  }
  
  showFiles("/");
}

/*
//timer dividers:
#define TIM_DIV1  0 //80MHz (80 ticks/us - 104857.588 us max)
#define TIM_DIV16 1 //5MHz (5 ticks/us - 1677721.4 us max)
#define TIM_DIV265  3 //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
//timer int_types:
#define TIM_EDGE  0
#define TIM_LEVEL 1
//timer reload values:
#define TIM_SINGLE  0 //on interrupt routine you need to write a new value to start the timer again
#define TIM_LOOP  1 //on interrupt the counter will start with the same value again
*/
void setupTimer1()
{  
  /* init timer interrupt to control the LEDs and the piezo */
  timer1_isr_init();
  timer1_attachInterrupt(controlOutputs);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(usToTicks(1000000/PIEZO_FREQUENCY));
}

/** ===========================================================
 * \fn      loop
 * \brief   standard Arduino forever loop-function with the
 *          state machine
 *
 * \param   -
 * \return  -
 ============================================================== */
void loop()
{
  event = getButtonEvent();
  switch (event)
  {
    case EVENT_SHORTCLICK:
    {
      Serial.println("EVENT_SHORTCLICK");
    }
    break;
    case EVENT_LONGCLICK:
    {
      Serial.println("EVENT_LONGCLICK");
    }
    break;
    case EVENT_ULTRALONGCLICK:
    {
      Serial.println("EVENT_ULTRALONGCLICK");

      /* turn off */
      Serial.println("going to sleep...");
      delay(1000);
      digitalWrite(WAKEUP_EN, LOW); // enable reset with button   //blup unnötig aber informativ
      ESP.deepSleep(0, WAKE_RF_DEFAULT); // in us!!!
      delay(1000);
    }
    break;
    case EVENT_NONE:
    {

    }
    break;
    /* error! */  
    default: while (FOREVER) displayError();
  }

  if (WiFi.status() == WL_CONNECTED) mdns.update();
  server.handleClient();
  delay(STD_DELAY);
}

/** ===========================================================
 * \fn      controlOutputs
 * \brief   this timer interrupt handler will be called every
 *          (1 / PIEZO_FREQUENZY)
 *          it controls the LEDs and the piezo
 *
 * \param   -
 * \return  -
 ============================================================== */
void controlOutputs()
{
//  noInterrupts();

/* control LEDs */
  /* increase global LED phase counter */
  phaseInterruptCounterLED++; 
  
  /* turn LEDs on or off on base of current desired brightnesses (duty cycles) */
  if (led.r >= phaseInterruptCounterLED) digitalWrite(LED_RED, LOW);
  else digitalWrite(LED_RED, HIGH);
  
  if (led.g >= phaseInterruptCounterLED) digitalWrite(LED_GREEN, LOW);
  else digitalWrite(LED_GREEN, HIGH);
  
  if (led.b >= phaseInterruptCounterLED) digitalWrite(LED_BLUE, LOW);
  else digitalWrite(LED_BLUE, HIGH);
  
  /* reset timer interrupt counter after each period */
  if (phaseInterruptCounterLED >= DC_100) phaseInterruptCounterLED = 0;

/* control piezo */
  static boolean togglePiezo = true;

  /* increase global timer interrupt counter */
  tickerCounterPiezo++;
  
  /* check if it is time to handle the piezo */
  if (tickerCounterPiezo >=  piezoFrequencyDivisor)
  {
    /* reset global piezo timer interrupt counter */
    tickerCounterPiezo = 0;
  
    /* toggle or turn off the piezo on base of current desired state */
    if (piezoOn && piezoEnable)
    {
      digitalWrite(PIEZO, togglePiezo);
      togglePiezo = !togglePiezo;
    }
    else digitalWrite(PIEZO, LOW);
  }

//  interrupts();
}

/** ===========================================================
 * \fn      getButtonEvent
 * \brief   returns given values on base of current button
 *          state
 *          (contains a while-loop as long as button is pushed)
 *
 * \param   -
 *
 * \return  (byte) 0 = no event
 *                 1 = short button click
 *                 2 = long button click
 *                 3 = ultra long button click 
 ============================================================== */
byte ICACHE_FLASH_ATTR getButtonEvent()
{
  byte buttonEvent = EVENT_NONE;
  unsigned int dt; // in ms
  
  /* check if pressed */
  if (buttonPressed())
  {
    /* turn piezo off */
    piezoOn = false;

    unsigned long ref = millis();  // get number of ms since the program runs as reference
    
   /* wait as long as the button is pressed and display something */
    while (buttonPressed())
    {
      dt = millis() - ref;
      
      if (dt < MAX_DURATION_SHORTCLICK) 
      {
        /* turn a LED on during short button click as feedback */
        setLEDs(COLOR_SHORT_BUTTON_CLICK, (DC_100 / CLICK_LED_BRIGHTNESS_DIVISOR));
      }
      else
      {
        if (dt >= MIN_DURATION_ULTRALONGCLICK)
        {
          /* turn a LED on during ultra long button click as feedback */
          setLEDs(COLOR_ULONG_BUTTON_CLICK, (DC_100 / CLICK_LED_BRIGHTNESS_DIVISOR));
        }
        else
        {
          /* turn a LED on during long button click as feedback */
          setLEDs(COLOR_LONG_BUTTON_CLICK, (DC_100 / CLICK_LED_BRIGHTNESS_DIVISOR));
        }
      }

      yield();
    }
    resetLEDs();

    /* calculate past time and set event value */
    dt = millis() - ref;
    if (dt <= MAX_DURATION_SHORTCLICK)
    {
      /* short click: */
      buttonEvent = 1;  // set current event
    }
    else 
    {
      /* check if it was an ultra long click */
      if (dt > MIN_DURATION_ULTRALONGCLICK)
      {
        /* ultra long click: */
        buttonEvent = 3;  // set current event
      }
      else 
      {
        /* long click: */
        buttonEvent = 2;  // set current event
      }
    }
  }

  return buttonEvent;
}

/** ===========================================================
 * \fn      buttonPressed
 * \brief   returns current button state
 *
 * \param   -
 * \return  (bool) true = button is pressed
 *                 false = button is not pressed
 ============================================================== */
boolean buttonPressed()
{ 
  return (!digitalRead(BUTTON));
}

/** ===========================================================
 * \fn      setupPossibleClientRequests
 * \brief   setup ESP8266 as a server
 *
 * \param   -
 * \return  -
 ============================================================== */
void setupPossibleClientRequests()
{
  /* api requests */
  server.on(URI_API_GPIO_LEDS, HTTP_GET,  handleLeds_get);
  server.on(URI_API_GPIO_LEDS, HTTP_POST, handleLeds_set);
  
  server.on(URI_API_CONFIG_SSID, HTTP_GET,  handleConfigSSID_get);
  server.on(URI_API_CONFIG_SSID, HTTP_POST, handleConfigSSID_set);
  server.on(URI_API_CONFIG_RESET, HTTP_GET,  handleConfigReset_get);
  //server.on(URI_API_CONFIG_RESET, HTTP_POST, handleConfigReset_set);

  server.on(URI_API_TIME, HTTP_GET,  handleTime_get);
  server.on(URI_API_TIME, HTTP_POST, handleTime_set);

  server.onFileUpload(handleFileUpload);
  server.on(URI_API_UPLOAD_FIRMWARE, HTTP_GET,  handleUploadFirmware_get);
  server.on(URI_API_UPLOAD_FIRMWARE, HTTP_POST, handleUploadFirmware_set);
  server.on(URI_API_UPLOAD_PATH, HTTP_GET,  handleUploadPath_get);
  server.on(URI_API_UPLOAD_PATH, HTTP_POST, handleUploadPath_set);
  server.on(URI_API_UPLOAD_FILE, HTTP_GET,  handleUploadFile_get);
  server.on(URI_API_UPLOAD_FILE, HTTP_POST, handleUploadFile_set);

//  server.on(URI_API_UPLOAD_FILE, HTTP_PUT, handleUploadFile_update);
//  server.on(URI_API_UPLOAD_FILE, HTTP_DELETE, handleUploadFile_delete);

  /* static requests */
  server.on(URI_INDEX_HTML, HTTP_GET,  handleRoot);  // root
//  server.serveStatic(URI_INDEX_HTML, SPIFFS, PATH_INDEX_HTML); // doesn't work
  server.serveStatic(URI_STYLES_CSS, SPIFFS, PATH_STYLES_CSS);
  server.serveStatic(URI_APP_JS, SPIFFS, PATH_APP_JS);
  server.serveStatic(URI_AWC_SIDE_HTML, SPIFFS, PATH_AWC_SIDE_HTML);
  server.serveStatic(URI_WELCOME_HTML, SPIFFS, PATH_WELCOME_HTML);
  server.serveStatic(URI_SAMPLE_HTML, SPIFFS, PATH_SAMPLE_HTML);
  server.serveStatic(URI_SETTINGS_HTML, SPIFFS, PATH_SETTINGS_HTML);
  
  server.onNotFound(handleNotFound);  
}

void handleRoot()
{
  staticServerResponse(PATH_INDEX_HTML, "text/html");
}

/* api request handlers */
void handleLeds_get()
{
  const byte numberOfJsonKeys = 3;
  
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
  JsonObject& jsonData = jsonDataBuffer.createObject();

  /* set json data variable according to the request and response to the client */
  jsonData["red"] = led.r;
  jsonData["green"] = led.g;
  jsonData["blue"] = led.b;
  
  sendToClientJsonData(jsonData);
}

void handleLeds_set()
{  
  const char* key0 = "red";
  const char* key1 = "green";
  const char* key2 = "blue";

  if (server.hasArg(key0) && server.hasArg(key1) && server.hasArg(key2))
  {
    /* get arguments */
    unsigned int keyValue0 = server.arg(key0).toInt();
    Serial.print("key0 = "); Serial.println(key0);
    Serial.print("keyValue0 = "); Serial.println(keyValue0);
    unsigned int keyValue1 = server.arg(key1).toInt();
    Serial.print("key1 = "); Serial.println(key1);
    Serial.print("keyValue1 = "); Serial.println(keyValue1);
    unsigned int keyValue2 = server.arg(key2).toInt();
    Serial.print("key2 = "); Serial.println(key2);
    Serial.print("keyValue2 = "); Serial.println(keyValue2);

    /* set output according to the request */
    setLEDs(keyValue0, keyValue1, keyValue2, DC_100);
    sendToClientJsonSuccess();
  }
  else sendToClientJsonInvalid(BAD_REQUEST);
}

void handleConfigSSID_get()
{
  const byte numberOfJsonKeys = 2;
  
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
  JsonObject& jsonData = jsonDataBuffer.createObject();

  /* set json data variable according to the request and response to the client */
  jsonData["ssid"] = (const char*)ssid;
  jsonData["ssid_pw"] = (const char*)ssid_pw;
  
  sendToClientJsonData(jsonData);
}

void handleConfigSSID_set()
{
  const char* key0 = "ssid";
  const char* key1 = "ssid_pw";
  
  boolean invalidSSID = false;
  boolean invalidSSID_pw = false;
  boolean connectToWLAN = false;
  
  if (server.hasArg(key0) && server.hasArg(key1))
  {
    /* get arguments */
    String newSSID = server.arg(key0);
    String newSSID_pw = server.arg(key1);
    Serial.print("key0 = "); Serial.println(key0);
    Serial.print("key1 = "); Serial.println(key1);
    Serial.print("newSSID = "); Serial.println(newSSID);
    Serial.print("newSSID_pw = "); Serial.println(newSSID_pw);

    /* decode arguments */
    newSSID = decodeURL(newSSID);
    newSSID_pw = decodeURL(newSSID_pw);
    Serial.print("decoded newSSID = "); Serial.println(newSSID);
    Serial.print("decoded newSSID_pw = "); Serial.println(newSSID_pw);
  
    /* check validity */
    if (validSSIDString(newSSID, MAX_SIZE_SSID) && newSSID != "")
    {
      if (validSSIDString(newSSID_pw, MAX_SIZE_SSID_PW))
      {
        /* setup EEPROM */
        EEPROM.begin(MAX_SIZE_EEPROM);
        
        /* clear old ssid-data */
        for (byte i = 0; i < MAX_SIZE_SSID; i++) ssid[i] = 0;
        for (byte i = 0; i < MAX_SIZE_SSID_PW; i++) ssid_pw[i] = 0;
        
        /* replace the old with the new ones */
        for (byte i = 0; i < newSSID.length(); i++) ssid[i] = newSSID[i];
        for (byte i = 0; i < newSSID_pw.length(); i++) ssid_pw[i] = newSSID_pw[i];
  
        /* clear SSID-data (EEPROM RAM) */
        Serial.println("clear SSID-data (EEPROM RAM)");
        for (unsigned int i = 0; i < (MAX_SIZE_SSID + MAX_SIZE_SSID_PW); i++) EEPROM.write(i, 0);
        
        /* write new SSID-data to the EEPROM (Big-endian -> highest byte first) */
        Serial.println("write new SSID-data to the EEPROM");
        for (byte i = 0; i < newSSID.length(); i++) EEPROM.write(i, newSSID[i]);
        for (byte i = 0; i < newSSID_pw.length(); i++) EEPROM.write(MAX_SIZE_SSID + i, newSSID_pw[i]);
        
        /* commit to the EEPROM and release the RAM copy of the EEPROM contents */
        EEPROM.end();   // .end includes a .commit();
  
        /* setup EEPROM (again) */
        EEPROM.begin(MAX_SIZE_EEPROM);
  
        /* read saved SSID-data from the EEPROM */
        Serial.println("read saved SSID-data from the EEPROM");
        char savedSSID[MAX_SIZE_SSID] = {0};    // must be initalized with 0!
        char savedSSID_pw[MAX_SIZE_SSID_PW] = {0};
        for (byte i = 0; i < newSSID.length(); i++)
        {
          savedSSID[i] = EEPROM.read(i);
        }
        for (byte i = 0; i < newSSID_pw.length(); i++)
        {
          savedSSID_pw[i] = EEPROM.read(MAX_SIZE_SSID + i);
        }
        Serial.print("savedSSID = "); Serial.println(savedSSID);
        Serial.print("savedSSID_pw = "); Serial.println(savedSSID_pw);
  
        /* commit to the EEPROM and release the RAM copy of the EEPROM contents */
        EEPROM.end();   // .end includes a .commit();
        
        /* check if it was correctly saved */
        if (newSSID == savedSSID)
        {
          if (newSSID_pw == savedSSID_pw)
          {
            Serial.println("new SSID-data = saved SSID-data!");
            connectToWLAN = true; // setupWiFi() have to be called after the server response
          }
          else invalidSSID_pw = true;
        }
        else invalidSSID = true;
        
      }
      else invalidSSID_pw = true;
    }
    else invalidSSID = true;

    /* check invalidity */
    if (!invalidSSID && !invalidSSID_pw)
    {
      /* handle valid: */
      
      sendToClientJsonSuccess();
    
      if (connectToWLAN)
      {
        ///* stop client */
        //server.client().stop(); //blup
        
        /* try to connect to the WLAN */
        resetLEDs();
        setupWiFi();
      }
    }
    else
    {
      /* handle invalid: */
      
      sendToClientJsonInvalid(NOT_ACCEPTABLE);
  
      if (invalidSSID) server.send(NOT_ACCEPTABLE, "text/plain", "invalid SSID!");  //blup
      else server.send(NOT_ACCEPTABLE, "text/plain", "invalid SSID_PW!");
    }
  }
  else sendToClientJsonInvalid(BAD_REQUEST);
}

void handleConfigReset_get()
{
  /* setup EEPROM */
  EEPROM.begin(MAX_SIZE_EEPROM);
  
  /* clear EEPROM */
  Serial.println("clear EEPROM");
  for (unsigned int i = 0; i < MAX_SIZE_EEPROM; i++) EEPROM.write(i, 0);

  /* commit to the EEPROM and release the RAM copy of the EEPROM contents */
  EEPROM.end();   // .end includes a .commit();

  sendToClientJsonSuccess();

  Serial.println("restart device...");
  Serial.println("");
  ESP.restart();
}

void handleTime_get()
{
  const byte numberOfJsonKeys = 5;
  
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
  JsonObject& jsonData = jsonDataBuffer.createObject();
    
  /* setup UDP */
  Serial.println("starting UDP");
  udp.begin(UDP_PORT);
  Serial.print("Local port = "); Serial.println(udp.localPort());

  /* get a random server from the time server pool */
  WiFi.hostByName(ntpServerName, timeServerIP);
  
  /* send NTP packets to the time server until response is > 0 */
  int cb = 0;
  while(cb <= 0)
  {
    sendNTPpacket();  //blup sollte IPAddress übergen können
    delay(DELAY_PER_NTP_PACKAGE);  //blup wait some time
    cb = udp.parsePacket();
    //blup TODO: escape einbauen
  }
  Serial.println("");

  Serial.print("packet received!, length = "); Serial.println(cb);

  /* read data from packet */
  udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

  //the timestamp starts at byte 40 of the received packet and is four bytes,
  // or two words, long. First, esxtract the two words:
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = (highWord << 16) | lowWord;
  Serial.print("Seconds since Jan 1 1900 = " ); Serial.println(secsSince1900);

  // now convert NTP time into everyday time:
  Serial.print("Seconds since Jan 1 1970 (Unix myTime) = ");
  // Unix time starts on Jan 1 1970. In seconds, that's +2208988800s:
  const unsigned long seventyYears = 2208988800UL;
  // subtract seventy years:
  unsigned long epoch = secsSince1900 - seventyYears;
  // print Unix myTime:
  Serial.println(epoch);

  /* calculate hour */
  myTime.hr = (epoch % 86400L) / 3600;     // 86400 equals secs per day
  //Serial.print("hr = "); Serial.println(myTime.hr);
  
  /* calculate min */
  myTime.min = (epoch  % 3600) / 60;       // 3600 equals secs per minute
  //Serial.print("min = "); Serial.println(myTime.min);
  
  /* calculate sec */
  myTime.sec = epoch % 60;
  //Serial.print("sec = "); Serial.println(myTime.sec);
  
  /* set time string */
  myTime.timeString = ""; // clear
  if (myTime.hr < 10) myTime.timeString += "0";
  myTime.timeString += String(myTime.hr) + ":";
  if (myTime.min < 10) myTime.timeString += "0";
  myTime.timeString += String(myTime.min) + ":";
  if (myTime.sec < 10) myTime.timeString += "0";
  myTime.timeString += String(myTime.sec);

  /* print time */
  Serial.print("The UTC+0 time is ");    // UTC is the time at Greenwich Meridian (GMT)
  Serial.println(myTime.timeString);

  /* add a hour if Summer Time is selected */
  if (myTime.sumTime)
  {
    myTime.hr++;

    /* handle hour overflow */
    if (myTime.hr >= 24) myTime.hr -= 24;
  }

  /* handle UTC configuration: */
  /* calculate hr */
  int h = (int)myTime.UTC;        // e.g. (int)-3.30 = -3
  //Serial.print("h = "); Serial.println(h);
  myTime.hr += h;

  /* handle hour overflows */
  if (myTime.hr >= 24) myTime.hr -= 24;
  if (myTime.hr < 0) myTime.hr += 24;
  
  /* calculate min */
  int m;  
  if (myTime.UTC < 0) m = (myTime.UTC - h) * 100 - 0.5;   // e.g. 3.30 - 3 = 0.3 -> 0.3 * 100 = 30
  else m = (myTime.UTC - h) * 100 + 0.5;              // +/- 0.5 is to round (int-cast problem)
  
  //Serial.print("m = "); Serial.println(m);
  myTime.min += m;
  
  /* handle overflows */
  if (myTime.min >= 60)
  {
    myTime.min -= 60;
    
    myTime.hr++;
    if (myTime.hr >= 24) myTime.hr -= 24;
  }
  if (myTime.min < 0)
  {
    myTime.min += 60;
    
    myTime.hr--;
    if (myTime.hr < 0) myTime.hr += 24;
  }

  /* update time string */
  myTime.timeString = ""; // clear
  if (myTime.hr < 10) myTime.timeString += "0";
  myTime.timeString += String(myTime.hr) + ":";
  if (myTime.min < 10) myTime.timeString += "0";
  myTime.timeString += String(myTime.min) + ":";
  if (myTime.sec < 10) myTime.timeString += "0";
  myTime.timeString += String(myTime.sec);

  /* print local time */
  Serial.print("The local time is ");
  Serial.println(myTime.timeString);

  /* set json data variables according to the request and response to the client */
  jsonData["hr"] = myTime.hr;
  jsonData["min"] = myTime.min;
  jsonData["sec"] = myTime.sec;
  jsonData["utc"] = myTime.UTC;
  jsonData["sumTime"] = (bool)myTime.sumTime;
  
  sendToClientJsonData(jsonData);
}

// send an NTP request to the time server at the given address
void sendNTPpacket()
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void handleTime_set()
{
  const char* key0 = "utc";       // float
  const char* key1 = "sumTime";   // bool

  if (server.hasArg(key0) && server.hasArg(key1))
  {
    /* get arguments */
    float keyValue0 = server.arg(key0).toFloat();
    String keyValue1_str = server.arg(key1);
    boolean keyValue1;
    if (keyValue1_str == "SUM") keyValue1 = true;
    else if (keyValue1_str == "STD") keyValue1 = false;
    else
    {
      sendToClientJsonInvalid(NOT_ACCEPTABLE);
      return;
    }
    
    Serial.print("key0 = "); Serial.println(key0);
    Serial.print("key1 = "); Serial.println(key1);
    Serial.print("keyValue0 = "); Serial.println(keyValue0);
    Serial.print("keyValue1 = "); Serial.println(keyValue1);
  
    /* check validity */
    if (abs(keyValue0) <= 14)  // |utc| <= 14 (max UTC = +14)
    {
      /* set output according to the request */
      myTime.UTC = keyValue0;
      myTime.sumTime = keyValue1;
      Serial.print("myTime.UTC = "); Serial.println(myTime.UTC);  //blup
      Serial.print("myTime.sumTime = "); Serial.println(myTime.sumTime);  //blup

      sendToClientJsonSuccess();
    }
    else sendToClientJsonInvalid(NOT_ACCEPTABLE);
  }
  else sendToClientJsonInvalid(BAD_REQUEST);
}

void handleFileUpload()
{
  /* get upload */
  HTTPUpload& upload = server.upload();
    
/* bin-file to reprogram your device -------------------------- */
  if (server.uri() == URI_API_UPLOAD_FIRMWARE)
  {
    /* handle start */
    if(upload.status == UPLOAD_FILE_START)
    {
      Serial.setDebugOutput(true);
      
      WiFiUDP::stopAll();
      Serial.printf("Upload: %s\n", upload.filename.c_str());
      
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      Serial.printf("max sketch space = %d bytes\n", maxSketchSpace);
      
      /* check if max sketch space is enough */
      if(!Update.begin(maxSketchSpace)) // start with max available size
      {
        Update.printError(Serial);
      }
    }
  
    /* handle upload */
    else if(upload.status == UPLOAD_FILE_WRITE)
    {
      if(Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      {
        Update.printError(Serial);
      }
    }
  
    /* handle end */
    else if(upload.status == UPLOAD_FILE_END)
    {
      if(Update.end(true))  // 'true' to set the size to the current progress
      {
        Serial.printf("Uploaded skech size = %u bytes\n", upload.totalSize);
      }
      else
      {
        Update.printError(Serial);
      }
      
      Serial.setDebugOutput(false);
    }
  }
/* ------------------------------------------------------------ */

/* file to save into the FS ----------------------------------- */
  else if (server.uri() == URI_API_UPLOAD_FILE)
  {
    /* handle start */
    if(upload.status == UPLOAD_FILE_START)
    {
      /* prepare path */
      pathSavedFile = pathSaved + "/" + upload.filename;

      /* check if file already exists */
      Serial.println("check if file already exists");
      File file = SPIFFS.open(pathSavedFile, "r");
      if (file)
      {
        /* remove file (necessary since we will append and not write the data) */
        Serial.println("file already exists -> remove");
        SPIFFS.remove(pathSavedFile);
      }
      else Serial.println("file does not exist yet");
      file.close();

      Serial.setDebugOutput(true);
      Serial.printf("start upload\n");
      
      WiFiUDP::stopAll();
      
      Serial.printf("Save file in: %s\n", pathSavedFile.c_str());
      
//      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
//      Serial.printf("max sketch space = %d bytes\n", maxSketchSpace);
//      
//      /* check if max sketch space is enough */
//      if(!Update.begin(maxSketchSpace)) // start with max available size
//      {
//        Update.printError(Serial);
//      }
    }
  
    /* handle upload */
    else if(upload.status == UPLOAD_FILE_WRITE)
    {
      Serial.printf("handle upload\n");
     
//      /* save data into a string */ //blup etwas speicherunfreundlich aber naja...
//      for (unsigned long i = 0; i < upload.currentSize; i++)
//      {
//        dataString += (char)upload.buf[i];
//      }

      /* save data directly into the file */
      File file = SPIFFS.open(pathSavedFile, "a");
      if (!file) {Serial.println("Failed to open the file for appending!"); while(1);}
      for (unsigned long i = 0; i < upload.currentSize; i++)
      {
        file.write((char)upload.buf[i]);
      }
      file.close();
    }
  
    /* handle end */
    else if(upload.status == UPLOAD_FILE_END)
    {
      Serial.printf("end upload\n");

//      Serial.println("print string content:");
//      Serial.println(dataString);


      showFileContent(pathSavedFile);

      
//      /* open file to write */
//      Serial.println("open file to write");
//      File file = SPIFFS.open(pathSavedFile, "w");
//      if (!file) {Serial.println("Failed to open the file for writing!"); while(1);}
//      Serial.print("file size before writing = ");  Serial.println(getFileSize(pathSavedFile));
//
//      /* write dataString into the file */
//      Serial.println("write dataString into the file");
//      file.print(dataString);
//      file.close();

      Serial.print("file size after writing = ");  Serial.println(getFileSize(pathSavedFile));

//      dataString.remove(0); // clear string (use .remove() for a proper removal!)
      
      Serial.setDebugOutput(false);
    }
  }
/* ------------------------------------------------------------ */
  else
  {
    sendToClientJsonInvalid(BAD_REQUEST);
    return;
  }
  
  yield();
}

void handleUploadFirmware_get()
{
  const byte numberOfJsonKeys = 1;
  
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
  JsonObject& jsonData = jsonDataBuffer.createObject();

  /* set json data variables according to the request and response to the client */
  jsonData["version"] = currentFirmwareVersion;
  
  sendToClientJsonData(jsonData);
}

void handleUploadFirmware_set()
{
  if (Update.hasError())
  {
    Serial.println("update program failed!");
    
    sendToClientJsonInvalid(NOT_ACCEPTABLE);
  }
  else
  {  
    Serial.println("update program successed!");
    
    /* set json data variable according to the request and response to the client */
    sendToClientJsonSuccess();

    delay(2000);  //blup
  
    Serial.println("restart device...");
    Serial.println("");
    ESP.restart();
  }
}

void handleUploadPath_get()
{
  const byte numberOfJsonKeys = 1;
  
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
  JsonObject& jsonData = jsonDataBuffer.createObject();

  /* make a char array since the jsonObject doesn't like Strings */
  char pathSavedBuf[pathSaved.length()+1];
  pathSaved.toCharArray(pathSavedBuf, sizeof(pathSavedBuf));

  /* set json data variables according to the request and response to the client */
  jsonData["path"] = pathSavedBuf;
  
  sendToClientJsonData(jsonData);
}

void handleUploadPath_set()
{
  const char* key0 = "path";
  
  if (server.hasArg(key0))
  {
    /* get argument */
    String keyValue0 = server.arg(key0);
    Serial.print("key0 = "); Serial.println(key0);
    Serial.print("keyValue0 = "); Serial.println(keyValue0);

    /* decode argument */
    keyValue0 = decodeURL(keyValue0);
    Serial.print("decoded keyValue0 = "); Serial.println(keyValue0);
  
    /* check validity */
    if (keyValue0[0] == '/' && keyValue0[keyValue0.length()-1] != '/') // first char is a '/' and last char isn't a '/')
    {
      /* set output according to the request */
      pathSaved = keyValue0;
      Serial.print("savedPath = "); Serial.println(pathSaved);  //blup

      sendToClientJsonSuccess();
    }
    else sendToClientJsonInvalid(NOT_ACCEPTABLE);
  }
  else sendToClientJsonInvalid(BAD_REQUEST);
}

void handleUploadFile_get()
{
  // blup: todo -> show or stream file instead of json
  Serial.println("blup");
  
  const byte numberOfJsonKeys = 1;
  
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
  JsonObject& jsonData = jsonDataBuffer.createObject();

  /* make a char array since the jsonObject doesn't like Strings */
  char pathSavedFileBuf[pathSavedFile.length()+1];
  pathSavedFile.toCharArray(pathSavedFileBuf, sizeof(pathSavedFileBuf));

  /* set json data variables according to the request and response to the client */
  jsonData["path"] = pathSavedFileBuf;
  
  sendToClientJsonData(jsonData);
}

void handleUploadFile_set() // will be called after handleFileUpload()
{
  const byte numberOfJsonKeys = 2;
  
  Serial.print("pathSavedFile = ");  Serial.println(pathSavedFile); //blup
  
  Serial.println("open file to read");
  File file = SPIFFS.open(pathSavedFile, "r");
  Serial.print("file.size() = ");  Serial.println(file.size());
  if (!file)
  {
    Serial.println("saveing file failed!");
    
//    Serial.println("remove file");  //blup
//    SPIFFS.remove(pathSavedFile);
    
    sendToClientJsonInvalid(NOT_ACCEPTABLE);
  }
  else
  {
    Serial.println("saveing file successed!");

    StaticJsonBuffer<JSON_OBJECT_SIZE(numberOfJsonKeys)> jsonDataBuffer;
    JsonObject& jsonData = jsonDataBuffer.createObject();

    /* make a char array since the jsonObject doesn't like Strings */
    char pathSavedFileBuf[pathSavedFile.length()+1];
    pathSavedFile.toCharArray(pathSavedFileBuf, sizeof(pathSavedFileBuf));
    
    /* set json data variable according to the request and response to the client */
    Serial.print("pathSavedFile = "); Serial.println(pathSavedFileBuf);
    Serial.println("");
    jsonData["file"] = pathSavedFileBuf;
    jsonData["size"] = file.size();
    sendToClientJsonData(jsonData);

    showFiles("/");
  }
  file.close();

  pathSaved = PATH_SRV_FILES; //blup reset path after an upload
}

void staticServerResponse(String path, String type)
{
  /* open file to read */
  Serial.println("open " + path + " to read");
  File file = SPIFFS.open(path, "r");
  if (!file) {Serial.println("Failed to open file for reading!"); while(1);}
  Serial.print("file.size() = ");  Serial.println(file.size());
  
  /* prepare response string */
  String responseString = file.readString();
  file.close();

  /* send response */
  Serial.println("server.send()");
  Serial.println("");
  server.send(SUCCESS, type, responseString);
}

void handleNotFound()
{  
  String serverResponse = "Sending HTTP 404 (Not Found)\n";
  serverResponse += "URI: ";
  serverResponse += server.uri();
  serverResponse += "\nMethod: ";
  serverResponse += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  serverResponse += "\nArguments: ";
  serverResponse += server.args();
  serverResponse += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ )
  {
    serverResponse += " " + server.argName(i) + ": " + server.arg(i) + "\n";  // decodeURL(server.arg(i))
  }

  Serial.print(serverResponse);
//  server.send(404, "text/plain", serverResponse);

  sendToClientJsonInvalid(NOT_FOUND);
}

void sendToClientJsonData(JsonObject& jsonData)
{ 
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_OBJECT_SIZE(6+1)> jsonBuffer;    //blup max number of json keys = ~6
  JsonObject& json = jsonBuffer.createObject();

  /* set json data variables */
  json["status"] = "success";
  json["data"] = jsonData;
  
  /* get current client */
  WiFiClient currentClient = server.client();
  
  /* write the response to the client... */
  currentClient.println("HTTP/1.1 200 OK");
  currentClient.println("Content-Type: application/json");
  currentClient.println("Connection: close");
  currentClient.print("\n");

  //server.send(SUCCESS, "application/json", ???);  // ??? -> json-data

  Serial.println("send the json-data to the client:");
  json.prettyPrintTo(Serial);
  Serial.println("");
  Serial.println("");
  
  json.printTo(currentClient);  // some clients doesn't like .prettyPrintTo(...)
}

void sendToClientJsonSuccess()
{ 
  /* setup json data object for the server response */
  StaticJsonBuffer<JSON_DATA_BUFFER_SIZE> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  /* set json data variables */
  json["status"] = "success";
  
  /* get current client */
  WiFiClient currentClient = server.client();
  
  /* write the response to the client... */
  currentClient.println("HTTP/1.1 200 OK");
  currentClient.println("Content-Type: application/json");
  currentClient.println("Connection: close");
  currentClient.print("\n");

  Serial.println("send the json-data to the client:");
  json.prettyPrintTo(Serial);
  Serial.println("");
  Serial.println("");
  
  json.printTo(currentClient);  // some clients doesn't like .prettyPrintTo(...)
}

void sendToClientJsonInvalid(unsigned int httpStatusCode)
{
  /* setup json object for the server response */
  StaticJsonBuffer<JSON_DATA_BUFFER_SIZE> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  /* get current client */
  WiFiClient currentClient = server.client();

  switch (httpStatusCode)
  {
    case BAD_REQUEST:
    {
      json["status"] = "Bad Request";
      currentClient.println("HTTP/1.1 400 Bad Request");
    }
    break;
    case NOT_ACCEPTABLE:
    {
      json["status"] = "Not Acceptable";
      currentClient.println("HTTP/1.1 406 Not Acceptable");
    }
    break;
    case TOO_LARGE:
    {
      json["status"] = "Too Large";
      currentClient.println("HTTP/1.1 413 Request Entity Too Large");
    }
    break;
    default:  //case NOT_FOUND:
    {
      json["status"] = "Not Found";
      currentClient.println("HTTP/1.1 404 Not Found");
    }
  }
   
  /* write the response to the client... */
  currentClient.println("Content-Type: application/json");
  currentClient.println("Connection: close");
  currentClient.print("\n");

  Serial.println("send the json-data to the client:");
  json.prettyPrintTo(Serial);
  Serial.println("");
  Serial.println("");
  
  json.printTo(currentClient);  // some clients doesn't like .prettyPrintTo(...)
}

/** ===========================================================
 * \fn      setupWiFi
 * \brief   setup device in station mode and try to connect to
 *          a WLAN. If it fails, setup device as access point
 *
 * \param   -
 * \return  -
 ============================================================== */
void setupWiFi()
{
  boolean connectingFailed = false;
  String mDNS;

  /* check if ssid is default or empty */
  if (ssid == DEFAULT_SSID || ssid[0] == 0)
  {
    connectingFailed = true;
  }
  else
  {
    /* init WiFi (station mode) */
    Serial.println("init WiFi");
    WiFi.begin(ssid, ssid_pw);
  
    printWifiMode();
    Serial.println("");

    Serial.println("trying to connect to the WLAN");

    unsigned long startMillisConnect = millis();
    
    /* wait until connected */
    while ((WiFi.status() != WL_CONNECTED) && !connectingFailed)
    {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, LOW);
      delay(250);
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, HIGH);
      delay(250);
      Serial.print(".");

      /* handle loop-escape */
      if ((millis() - startMillisConnect) >= DELAY_UNTIL_SM_LOOP_ESCAPE)
      {
        Serial.println("");
        Serial.println("connecting failed -> setup device as access point");
        connectingFailed = true;
      }
    }
    Serial.println("");
  }

  if (connectingFailed)
  {
    /* setup softAP */
    Serial.println("setup access point");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(DEFAULT_SSID, DEFAULT_SSID_PW);  // a password is optional
    delay(1);
    printWifiMode();

    /* feedback connection failed */
    digitalWrite(LED_RED, LOW);
    delay(LED_FEEDBACK_DURATION);
    digitalWrite(LED_RED, HIGH);
  }
  else
  {
    Serial.println("connected!");
    Serial.println("");

    /* feedback connection successed */
    digitalWrite(LED_GREEN, LOW);
    delay(LED_FEEDBACK_DURATION);
    digitalWrite(LED_GREEN, HIGH);

    /* setup mDNS responder */
    mDNS = setupMDNSResponder();  // returns the mDNS string
    //Serial.println("");
    
    Serial.println("add mDNS service");
    Serial.println("");
    MDNS.addService("http", "tcp", HTTP_PORT);
    
    /* set station mode (teardown AP) */
    Serial.println("set wifi mode");
    WiFi.mode(WIFI_STA);
    printWifiMode();
  }
  Serial.println("");

  /* print some connection informations */
  Serial.println(DEFAULT_SSID);
  Serial.println("");
  
  Serial.print("SSID = ");
  Serial.println(ssid);
  Serial.print("ESP8266_mDNS = ");
  Serial.println(mDNS + ".local");
  Serial.print("ESP8266_Local-IP = ");
  Serial.println(WiFi.localIP());
  Serial.print("subnetMask = ");
  Serial.println(WiFi.subnetMask());
  Serial.print("gatewayIP = ");
  Serial.println(WiFi.gatewayIP());
  printMacAddress();
  Serial.println("");
  
  /* start the TCP (HTTP) server */
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("");
}

String setupMDNSResponder()
{
  Serial.println("setup mDNS responder");
  
  String mdnsName = MDNS_NAME;// + itoa(deviceID);
    
  char mdnsChars[MAX_SIZE_SSID];
  mdnsName.toCharArray(mdnsChars, MAX_SIZE_SSID);
  
  Serial.print("mDNS = "); Serial.println(mdnsName);
  if (!mdns.begin(mdnsChars))  // , WiFi.localIP()
  {
    Serial.println("error: setting up mDNS responder failed!");
    delay(1000);
    ESP.restart();
  }
  Serial.println("mDNS responder started");
  
  return mdnsName;
}

String decodeURL(String URL)
{
  String decodedURL = URL;

  /* replace all '+' with ' ' (spaces) */
  decodedURL.replace('+', ' ');

  /* replace all '%hex' with corresponding ACII values */
  while (URL.indexOf('%') != -1)  // indexOf('%') returns the pos of '%' (0... str_length-1) or -1
  {
    /* isolate hex value */
    int hexPos = URL.indexOf('%') + 1;
    String hex = URL.substring(hexPos, hexPos + 2);

    /* convert to a vlaue between 0 and 255 */
    byte dec = hex2dec(hex);

    /* convert value to the corresponding ASCII value and make a string */
    char ascii[2];
    sprintf(ascii, "%c", dec);
    
    /* replace characters for the decoded string */
    decodedURL.replace('%' + hex, ascii);
    
    /* remove last '%xx' from input string */
    URL = URL.substring(hexPos + 2, URL.length());
  }

  return decodedURL;
}

byte hex2dec(String hex)
{
  /* convert String to char[] */
  char *hex_c = new char[2];
  strcpy(hex_c, hex.c_str());

  /* prepare hex characters for the conversion */
  if (hex_c[0] > 47 && hex_c[0] < 58) hex_c[0] -= 48;       // hex character is a number
  else if (hex_c[0] > 64 && hex_c[0] < 71) hex_c[0] -= 55;  // hex character is a letter between 'A' (= 10) and 'F' (= 15)
  
  if (hex_c[1] > 47 && hex_c[1] < 58) hex_c[1] -= 48;
  else if (hex_c[1] > 64 && hex_c[1] < 71) hex_c[1] -= 55;

  /* convert & return */  
  return (hex_c[0] * 16 + hex_c[1]);
}

boolean validSSIDString(String s, unsigned int maxLength)
{
  boolean valid = true;

  /* invalid length */
  if (s.length() > maxLength) valid = false;

  /* invalid character */
  if ((s.indexOf('"') != -1) || (s.indexOf('<') != -1) || (s.indexOf('>') != -1) ||
      (s.indexOf('\'') != -1) || (s.indexOf('&') != -1) ) valid = false;

  return valid;
}

//String itoa(int i)
//{
//  char s[11] = {0};   // (uint) 2^31-1 = +/-2'147'483'647 (max = 11 positions with +/-)
//  sprintf(s, "%d", i);
//  
//  return s;
//}
//
//String ftoa(float f, byte decimalPlaces)
//{
//  unsigned int ipart = (unsigned int)f;
//  float fpart = f - (float)ipart;
//
//  /* convert integer part to string */
//  String s = itoa(ipart);
//
//  /* add dot */
//  s += '.';
//
//  /* get defined number of decimal places */
//  fpart *= pow(10, decimalPlaces);
//
//  /* convert float part to string */
//  s += itoa((unsigned int)fpart);
//  
//  return s;
//}

void printWifiMode()
{
  const char *wifiModes[] = {"NULL", "STA", "AP", "STA+AP"};
  Serial.print("WiFi mode = ");
  Serial.println(wifiModes[wifi_get_opmode()]);
}

void printSleepType()
{
  const char *types[] = {"NONE", "LIGHT", "MODEM"};
  Serial.print("sleep type = ");
  Serial.println(types[wifi_get_sleep_type()]);
}

void printMacAddress()
{
  uint8 m[6] = {0};
  char macStr[18] = {0};

  if (wifi_get_opmode() == 2) wifi_get_macaddr(SOFTAP_IF, m); // SoftAP mode
  else wifi_get_macaddr(STATION_IF, m); // default
  
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],  m[1], m[2], m[3], m[4], m[5]);
  
  Serial.print("mac address = ");
  Serial.println(macStr);
}

void showEEPROM()
{
  Serial.println("show EEPROM content:");
  EEPROM.begin(MAX_SIZE_EEPROM);
  char s[MAX_SIZE_EEPROM] = {0};
  for (byte i = 0; i < MAX_SIZE_SSID; i++) s[i] = EEPROM.read(i);                     // read SSID
  Serial.print("SSID = "); Serial.println(s);
  for (byte i = 0; i < MAX_SIZE_SSID; i++) s[i] = 0;                                  // clear string
  for (byte i = 0; i < MAX_SIZE_SSID_PW; i++) s[i] = EEPROM.read(MAX_SIZE_SSID + i);  // read SSID_PW
  Serial.print("SSID_PW = "); Serial.println(s);
  //for (byte i = 0; i < MAX_SIZE_SSID_PW; i++) s[i] = 0;                               // clear string
  //s[0] = EEPROM.read(MAX_SIZE_SSID_PW);                                               // read devID
  //Serial.print("devID = "); Serial.println(int(s[0]), DEC);
  Serial.println("");
  EEPROM.end();
}

// returns total number of files
unsigned int showFiles(String path)
{
  Dir dir = SPIFFS.openDir(path);
  Serial.println("show files in \"" + path + "\":");
  unsigned int n;
  unsigned int m = 0;
  for (n = 0; dir.next(); n++)
  {
    Serial.print("dir = "); Serial.print(dir.fileName());
    File testFile = dir.openFile("r");
    Serial.print("    size = "); Serial.println(testFile.size());
    m += testFile.size();
    testFile.close();
  }
  Serial.print("total number of files = "); Serial.println(n);
  Serial.print("total used memory = "); Serial.println(m);
  Serial.println("");

  return n;
}

void showFileContent(String filePath)
{
  File file = SPIFFS.open(filePath, "r");
  if (!file) while(1);

  Serial.println("show file content:");
  Serial.println(file.readString());
  Serial.println("");
  
  file.close();
}

void showFileSize(String filePath)
{
  File file = SPIFFS.open(filePath, "r");
  if (!file) while(1);

  Serial.print("file size = "); Serial.println(file.size());
  Serial.println("");
  
  file.close();
}

//returns file size or -1 if non existent
int getFileSize(String filePath)
{
  int fSize;
  
  File file = SPIFFS.open(filePath, "r");
  if (!file) fSize = -1;
  else fSize = file.size();
  
  file.close();

  return fSize;
}

/** ===========================================================
 * \fn      displayError
 * \brief   display something if an error occured
 *
 * \param   -
 * \return  -
 ============================================================== */
void ICACHE_FLASH_ATTR displayError()
{
  setLEDs(RED, DC_25);
  delay(200);
  resetLEDs();
  delay(200);
}

/** ===========================================================
 * \fn      waitRemainingMillis
 * \brief   wait given remaining duration in milli seconds
 *
 * \requ    global variable: unsigned long startMillis
 *
 * \param   (ulong) time to wait in milli seconds
 * \return  -
 ============================================================== */
void waitRemainingMillis(unsigned long ms)
{
  while ((millis() - startMillis) < ms) yield();
  
  startMillis = millis();
}

/** ===========================================================
 * \fn      waitRemainingMicros
 * \brief   wait given remaining duration in micro seconds
 *
 * \requ    global variable: unsigned long startMicros
 *
 * \param   (ulong) time to wait in micro seconds
 * \return  -
 ============================================================== */
void waitRemainingMicros(unsigned long us)
{
  while ((micros() - startMicros) < us) yield();
  
  startMicros = micros();
}

 /** ===========================================================
  * \fn      waitRemainingMillisConditionally
  * \brief   wait given remaining duration but leaves immediately
  *          when given digital input pin becomes 0 (false) 
  *          returns 1 (true) if that was the case
  *
  * \requ    global variable: unsigned long startMillis
  *
  * \param   (ulong) duration to wait in ms
  *          (byte)  digital input pin to leave the while-loop
  * \return  (bool)  true if while-loop has been left
  ============================================================== */
 boolean waitRemainingMillisConditionally(unsigned long ms, byte inputPin)
 {
   while ((millis() - startMillis) < ms && digitalRead(inputPin)) yield(); // low active!

   startMillis = millis();
   
   if (!digitalRead(inputPin)) return true;
   else return false;
 }

/** ===========================================================
 * \fn      waitConditionally
 * \brief   wait given duration but leaves immediately when
 *          given digital input pin becomes 0 (false) 
 *          returns 1 (true) if that was the case
 *
 * \param   (ulong) duration to wait in ms
 *          (byte)  digital input pin to leave the while-loop
 * \return  (bool)  true if while-loop was left
 ============================================================== */
boolean waitConditionally(unsigned long duration, byte inputPin)
{
  unsigned long ref = millis();
  while ((millis() - ref) < duration && digitalRead(inputPin)) yield(); // low active!
  
  if (!digitalRead(inputPin)) return true;
  else return false;
}

/** ===========================================================
 * \fn      piezo
 * \brief   plays a tone
 *
 * \param   (byte) piezo frequency divisor
 * \return  -
 ============================================================== */
void piezo(byte fd)
{ 
  if (fd > 0)
  {
    piezoOn = true;
    piezoFrequencyDivisor = fd;
  }
  else piezoOn = false;
}

/** ===========================================================
 * \fn      setLEDs
 * \brief   set RGB-LEDs
 *
 * \param   (uint) red (255 = max)
 *          (uint) green (255 = max)
 *          (uint) blue (255 = max)
 *          (uint) overall brightness (DC_100 = max)
 * \return  -
 ============================================================== */
void setLEDs(unsigned int r, unsigned int g, unsigned int b, unsigned int brightness)
{
  led.r = r * brightness / 255;  // = r/255 * (0... 20)
  led.g = g * brightness / 255;
  led.b = b * brightness / 255;
}

/** ===========================================================
 * \fn      resetLEDs
 * \brief   reset RGB-LEDs
 *
 * \param   -
 * \return  -
 ============================================================== */
void resetLEDs()
{  
  led.r = 0;
  led.g = 0;
  led.b = 0;
}

/** ===========================================================
 * \fn      resetAllOutputs
 * \brief   resets all outputs
 *
 * \param   -
 * \return  -
 ============================================================== */
void resetAllOutputs()
{
  resetLEDs();
  piezoOn = false;
}


const unsigned int usToTicks(unsigned int us)
{
  return (clockCyclesPerMicrosecond() / 16 * us);     // converts microseconds to tick
}

//const unsigned int ticksToUs(unsigned int ticks)
//{
//    return (ticks / clockCyclesPerMicrosecond() * 16);  // converts from ticks back to microseconds
//}

