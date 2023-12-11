#include <AudioTools.h>
#include <AudioLibs/AudioKit.h>
#include <AudioLibs/AudioSourceSD.h>
#include <AudioCodecs/CodecMP3Helix.h>
#include <DNSServer.h>
#include <ESPUI.h>
#include <RTClib.h>
#include <ESP32Time.h>
#include <ESP32Servo.h>
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

RTC_DS3231 rtc;
ESP32Time espRTC(3600);

uint16_t timeInput, timeLabel;

void IRAM_ATTR coinInterrupt();
void checkCoinSignalEnd();
void coinDetected(int coin);
int loadFiles(const char *path, char files[MAX_FILES][MAX_FILE_PATH_LENGTH], int &count);
void playCoinSound(int coin);
void getTimeCallback(Control *sender, int type);
void syncTimeCallback(Control *sender, int type);
void updateTimeLabel();

TickTwo timeLabelUpdater(updateTimeLabel, 1000, 0, MILLIS);

sqlite3 *db;
sqlite3_stmt *res;
const char *data = "Callback function called";
char *zErrMsg = 0;
char sql[256];

static int callback(void *data, int argc, char **argv, char **azColName)
{
  int i;
  Serial.printf("%s: ", (const char *)data);
  for (i = 0; i < argc; i++)
  {
    Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  Serial.printf("\n");
  return 0;
}

int db_exec(sqlite3 *db, const char *sql)
{
  Serial.printf("SQL query: %s\n", sql);
  int rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK)
  {
    Serial.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  return rc;
}

void setup()
{
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  ESPUI.setVerbosity(Verbosity::Quiet);

  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = true;
  kit.begin(cfg);

  AudioActions &actions = kit.audioActions();
  actions.setEnabled(PIN_KEY6, false);

  pinMode(PIN_KEY6, INPUT_PULLUP);
  attachInterrupt(PIN_KEY6, coinInterrupt, CHANGE);

  SD.begin(PIN_AUDIO_KIT_SD_CARD_CS, AUDIOKIT_SD_SPI);

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

  sqlite3_initialize();
  if (sqlite3_open(DATABASE_PATH, &db) != SQLITE_OK)
  {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
  }

  timeLabel = ESPUI.label("Date & Time", ControlColor::Sunflower, "");
  timeInput = ESPUI.addControl(Time, "", "", None, 0, getTimeCallback);
  ESPUI.addControl(Button, "Update time", "Sync with device", None, timeLabel, syncTimeCallback);
  ESPUI.begin("PIGGY BANK");

  timeLabelUpdater.start();
  srand(espRTC.getLocalEpoch());

  playCoinSound(1);
}

void loop()
{
  dnsServer.processNextRequest();
  kit.processActions();
  player.copy();
  timeLabelUpdater.update();
  checkCoinSignalEnd();
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
  lastHighSignal = millis();
  if (millis() - lastLowSignal != COIN_SIGNAL_LENGTH)
    return;
  coinPulses++;
}

void checkCoinSignalEnd()
{
  if ((coinPulses > 0) && (millis() - lastLowSignal > COIN_SIGNAL_LENGTH) && (millis() - lastHighSignal > COIN_SIGNAL_PAUSE_LENGTH))
  {
    coinDetected(coinPulses);
    coinPulses = 0;
  }
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

void getTimeCallback(Control *sender, int type)
{
  if (type == TM_VALUE)
  {
    Serial.println("ISO time from device " + sender->value);
    const char *ISO8601Time = sender->value.c_str();

    DateTime dt(ISO8601Time);
    rtc.adjust(dt);

    espRTC.setTime(rtc.now().unixtime());
  }
}

void syncTimeCallback(Control *sender, int type)
{
  if (type == B_DOWN)
  {
    ESPUI.updateTime(timeInput);
  }
}

void updateTimeLabel()
{
  ESPUI.updateLabel(timeLabel, espRTC.getTimeDate());
}
