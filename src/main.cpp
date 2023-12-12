#include <AudioTools.h>
#include <AudioLibs/AudioKit.h>
#include <AudioLibs/AudioSourceSD.h>
#include <AudioCodecs/CodecMP3Helix.h>
#include <DNSServerAsync.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <ESP32Time.h>
#include <TickTwo.h>
#include <sqlite3.h>

#define MAX_FILES 32
#define MAX_FILE_PATH_LENGTH 128
#define MAX_COIN_TYPES 6
#define COIN_SIGNAL_LENGTH 50
#define COIN_SIGNAL_PAUSE_LENGTH 100

#define COIN_SOUND_FOLDER_PATH "/sound/coin"
#define DATABASE_PATH "/sd/database.db"

float coinValues[MAX_COIN_TYPES] = {2, 1.0, 0.50, 0.20, 0.10, 0.05};

AudioSourceSD source(COIN_SOUND_FOLDER_PATH, "mp3", PIN_AUDIO_KIT_SD_CARD_CS);
AudioKitStream kit;
MP3DecoderHelix decoder;
AudioPlayer player(source, kit, decoder);

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
DNSServer dnsServer;

AsyncWebServer web(80);
AsyncWebSocket ws("/ws");

RTC_DS3231 rtc;
ESP32Time espRTC(3600);

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
      return request->send(LittleFS, "/index.html", "text/html");
    if (LittleFS.exists(url))
      return request->send(LittleFS, url);
    if (SD.exists(url))
      return request->send(SD, url);
    request->redirect("/");
  }
};

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT))
    return;

  StaticJsonDocument<64> doc;
  DeserializationError err = deserializeJson(doc, data);

  if (err)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return;
  }

  const char *type = doc["type"];
  const char *message = doc["message"];

  if (strcmp(type, "UpdateDateTime") == 0)
  {
    DateTime dt(message);
    rtc.adjust(dt);
    espRTC.setTime(rtc.now().unixtime());
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
  StaticJsonDocument<64> doc;
  doc["type"] = "CurrentDateTime";
  doc["message"] = espRTC.getTime("%d-%m-%Y %H:%M:%S");
  size_t len = serializeJson(doc, data);
  ws.textAll(data, len);
}

TickTwo notifyDateTimeTicker(notifyCurrentDateTime, 1000, 0, MILLIS);
sqlite3 *db;
char sql[512], *sqlError;

static int callback(void *data, int argc, char **argv, char **azColName)
{
  for (int i = 0; i < argc; i++)
  {
    Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  Serial.printf("\n");
  return 0;
}

int db_exec(sqlite3 *db, const char *sql)
{
  Serial.printf("SQL query: %s\n", sql);
  int rc = sqlite3_exec(db, sql, callback, (void *)NULL, &sqlError);
  if (rc != SQLITE_OK)
  {
    Serial.printf("SQL error: %s\n", sqlError);
    sqlite3_free(sqlError);
  }
  return rc;
}

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

  player.setPath(files[rand() % count]);
  player.play();
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
  if (millis() - lastLowSignal != COIN_SIGNAL_LENGTH)
    return;
  lastHighSignal = millis();
  coinPulses++;
}

void coinDetected(int coin)
{
  Serial.printf("Detected coin: %d\n", coin);

  playCoinSound(coin);

  float amount = coinValues[coin - 1];
  const char *date = espRTC.getTime("%Y-%m-%d %H:%M:%S").c_str();
  sprintf(sql, "INSERT INTO donations (amount, time) VALUES( %f, \'%s\');", amount, date);

  db_exec(db, sql);
}

void checkCoinSignalEnd()
{
  if ((coinPulses > 0) && (millis() - lastLowSignal > COIN_SIGNAL_LENGTH) && (millis() - lastHighSignal > COIN_SIGNAL_PAUSE_LENGTH))
  {
    coinDetected(coinPulses);
    coinPulses = 0;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = true;
  kit.begin(cfg);

  AudioActions &actions = kit.audioActions();
  actions.setEnabled(PIN_KEY6, false);

  pinMode(PIN_KEY6, INPUT_PULLUP);
  attachInterrupt(PIN_KEY6, coinInterrupt, CHANGE);

  SD.begin(PIN_AUDIO_KIT_SD_CARD_CS, AUDIOKIT_SD_SPI);
  LittleFS.begin();

  kit.setVolume(90);
  player.setVolume(1.0);

  Wire.setPins(22, 21);
  if (!rtc.begin())
  {
    Serial.println("Could not find RTC module");
  }
  else
  {
    espRTC.setTime(rtc.now().unixtime());
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP("PIGGY BANK");

  dnsServer.start(DNS_PORT, "*", apIP);

  ws.onEvent(onEvent);
  web.addHandler(&ws).setFilter(ON_AP_FILTER);
  web.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  web.begin();

  sqlite3_initialize();
  if (sqlite3_open(DATABASE_PATH, &db) != SQLITE_OK)
  {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
  }

  srand(espRTC.getLocalEpoch());
  playCoinSound(1);

  notifyDateTimeTicker.start();
}

void loop()
{
  kit.processActions();
  player.copy();
  ws.cleanupClients();
  notifyDateTimeTicker.update();
  checkCoinSignalEnd();
}