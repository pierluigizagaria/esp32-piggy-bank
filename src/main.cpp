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

#define COIN_SOUND_FOLDER_PATH "/sound/coin"
#define DATABASE_PATH "/sd/database.db"
#define MAX_FILES 32

AudioSourceSD source(COIN_SOUND_FOLDER_PATH, "mp3", PIN_AUDIO_KIT_SD_CARD_CS);
AudioKitStream kit;
MP3DecoderHelix decoder;
AudioPlayer player(source, kit, decoder);

byte coinPulses = 0;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
DNSServer dnsServer;

RTC_DS3231 rtc;
ESP32Time espRTC(3600);

uint16_t timeInput, timeLabel;

sqlite3 *db;
sqlite3_stmt *res;

void IRAM_ATTR coinInterrupt();
void coinDetected(int coin);
void loadFiles(const char *path, char *files[], int &count);
void playCoinSound(int coin);
void getTimeCallback(Control *sender, int type);
void syncTimeCallback(Control *sender, int type);
void updateTimeLabel();

TickTwo timeLabelUpdater(updateTimeLabel, 1000, 0, MILLIS);

TickTwo pulsesWatchdog([]
                       {coinDetected(coinPulses); 
                        coinPulses = 0; },
                       150, 1, MILLIS);

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
  attachInterrupt(PIN_KEY6, coinInterrupt, RISING);

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
  pulsesWatchdog.update();
}

void IRAM_ATTR coinInterrupt()
{
  coinPulses++;
  pulsesWatchdog.start();
}

void coinDetected(int coin)
{
  Serial.print("Detected coin: ");
  Serial.println(coin);
  playCoinSound(coin);
}

void loadFiles(const char *path, char *files[], int &count)
{
  File directory = SD.open(path);

  if (!directory.isDirectory())
  {
    Serial.printf("Path %s does not exist\n", path);
    return;
  }

  count = 0;

  while (File entry = directory.openNextFile())
  {
    if (!entry.isDirectory())
    {
      if (count < MAX_FILES)
      {
        files[count] = strdup(entry.path());
        count++;
      }
    }
    entry.close();
  }
  directory.close();
}

void playCoinSound(int coin)
{
  char path[256];
  sprintf(path, "%s/%d", COIN_SOUND_FOLDER_PATH, coin);

  char *files[MAX_FILES];
  int count = 0;
  loadFiles(path, files, count);

  for (int i = 0; i < count; i++)
    Serial.printf("%d. %s\n", i, files[i]);

  player.setPath(files[rand() % count]);
  player.play();
}

void updateTimeLabel()
{
  ESPUI.updateLabel(timeLabel, espRTC.getTimeDate());
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
