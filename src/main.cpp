#include <AudioTools.h>
#include <AudioLibs/AudioKit.h>
#include <AudioLibs/AudioSourceSD.h>
#include <AudioCodecs/CodecMP3Helix.h>
#include <DNSServerAsync.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <TickTwo.h>
#include <LittleFS.h>

#define MAX_FILES 8
#define MAX_FILE_PATH_LENGTH 32
#define MAX_COIN_TYPES 6
#define COIN_SIGNAL_LENGTH 50
#define COIN_SIGNAL_TOLLERANCE 5
#define COIN_SIGNAL_PAUSE_LENGTH 100 + 10

#define COIN_SOUND_FOLDER_PATH "/sound/coin"
#define DONATIONS_FILE "/donations.csv"

float coinValues[MAX_COIN_TYPES] = {2, 1.0, 0.50, 0.20, 0.10, 0.05};

AudioKitStream kit;
EncodedAudioStream decoder(&kit, new MP3DecoderHelix());
StreamCopy copier;
File audioFile;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
DNSServer dnsServer;

AsyncWebServer web(80);
AsyncWebSocket ws("/ws");

RTC_DS3231 rtc;

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    String url = request->url();
    if (url == "/")
      request->send(LittleFS, "/index.html", "text/html");
    else if (LittleFS.exists(url) || SD.exists(url))
    {
      AsyncWebServerResponse *response;
      if (LittleFS.exists(url))
        response = request->beginResponse(LittleFS, url);
      else
        response = request->beginResponse(SD, url);
      if (request->url().endsWith(".wasm"))
        response->setContentType("application/wasm");
      request->send(response);
    }
    else
      request->redirect("/");
  }
};

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT))
    return;

  StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
  DeserializationError err = deserializeJson(doc, data);

  if (err)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return;
  }

  const char *type = doc["type"];

  if (strcmp(type, "UpdateDateTime") == 0)
  {
    uint32_t timestamp = doc["message"];
    DateTime dt(timestamp);
    rtc.adjust(dt);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void notifyCurrentDateTime()
{
  char data[128];
  StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
  doc["type"] = "CurrentDateTime";
  doc["message"] = rtc.now().unixtime();
  size_t len = serializeJson(doc, data);
  ws.textAll(data, len);
}

void notifyClients(const char *event)
{
  char data[128];
  StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
  doc["type"] = "Notification";
  doc["message"] = event;
  size_t len = serializeJson(doc, data);
  ws.textAll(data, len);
}

TickTwo notifyDateTimeTicker(notifyCurrentDateTime, 1000, 0, MILLIS);

int loadFiles(const char *path, char files[MAX_FILES][MAX_FILE_PATH_LENGTH], int &count)
{
  File directory = SD.open(path);

  if (!directory.isDirectory())
    return -1;

  count = 0;
  while (File entry = directory.openNextFile())
  {
    if (!entry.isDirectory())
    {
      if (count < MAX_FILES)
      {
        strcpy(files[count], entry.path());
        count++;
      }
    }
    entry.close();
  }
  directory.close();
  return 0;
}

void playCoinSound(int coin)
{
  char path[MAX_FILE_PATH_LENGTH];
  char files[MAX_FILES][MAX_FILE_PATH_LENGTH];
  int count = 0;

  sprintf(path, "%s/%d", COIN_SOUND_FOLDER_PATH, coin);
  if (loadFiles(path, files, count))
    return;

  for (int i = 0; i < count; i++)
    Serial.printf("%d. %s\n", i, files[i]);

  audioFile = SD.open(files[rand() % count]);
  copier.begin(decoder, audioFile);
}

byte coinPulses = 0;
unsigned long lastLowSignal, lastHighSignal;

void IRAM_ATTR coinInterrupt()
{
  int state = digitalRead(PIN_KEY6);
  if (state == LOW)
  {
    lastLowSignal = millis();
    return;
  }
  unsigned long pulseLength = millis() - lastLowSignal;
  if ((pulseLength > COIN_SIGNAL_LENGTH + COIN_SIGNAL_TOLLERANCE) || (pulseLength < COIN_SIGNAL_LENGTH - COIN_SIGNAL_TOLLERANCE))
    return;
  lastHighSignal = millis();
  coinPulses++;
}

void saveDonation(int coin)
{
  File donationsFile = SD.open(DONATIONS_FILE, FILE_APPEND);
  if (!donationsFile)
    return;
  donationsFile.printf("%f;%d;\n", coinValues[coin - 1], rtc.now().unixtime());
  donationsFile.close();
}

void checkCoinSignalEnd()
{
  if ((coinPulses > 0) && (millis() - lastLowSignal > COIN_SIGNAL_LENGTH) && (millis() - lastHighSignal > COIN_SIGNAL_PAUSE_LENGTH))
  {
    Serial.printf("Detected coin: %d\n", coinPulses);
    playCoinSound(coinPulses);
    saveDonation(coinPulses);
    notifyClients("DONATION_RECEIVED");
    coinPulses = 0;
  }
}

void printHeapStatus()
{
  auto freeHeap = esp_get_free_heap_size();
  auto minHeap = esp_get_minimum_free_heap_size();
  Serial.printf("[HEAP] free:%i, minimum:%i\n", freeHeap, minHeap);
}

TickTwo printHeapStatusTicker(printHeapStatus, 5000, 0, MILLIS);

void setup()
{
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = true;
  kit.begin(cfg);

  SD.begin(PIN_AUDIO_KIT_SD_CARD_CS, AUDIOKIT_SD_SPI);
  LittleFS.begin();

  kit.setVolume(90);
  decoder.begin();

  AudioActions &actions = kit.audioActions();
  actions.setEnabled(PIN_KEY6, false);

  pinMode(PIN_KEY6, INPUT_PULLUP);
  attachInterrupt(PIN_KEY6, coinInterrupt, CHANGE);

  Wire.setPins(22, 21);
  if (!rtc.begin())
  {
    Serial.println("Could not find RTC module");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP("PIGGY BANK", "apritisesamo");

  dnsServer.start(DNS_PORT, "*", apIP);

  ws.onEvent(onEvent);
  web.addHandler(&ws).setFilter(ON_AP_FILTER);
  web.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  web.begin();

  srand(rtc.now().unixtime());
  printHeapStatusTicker.start();
  notifyDateTimeTicker.start();
}

void loop()
{
  kit.processActions();
  ws.cleanupClients();
  printHeapStatusTicker.update();
  notifyDateTimeTicker.update();
  checkCoinSignalEnd();
  if (!copier.copy() && audioFile)
  {
    audioFile.close();
  }
}