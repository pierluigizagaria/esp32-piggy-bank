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
#include "sounds.h"

#define SOUND_FOLDER_PATH "/sound"
#define DATABASE_PATH "/sd/database.db"

AudioSourceSD source(SOUND_FOLDER_PATH, "mp3", PIN_AUDIO_KIT_SD_CARD_CS);
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

sqlite3 *db;
sqlite3_stmt *res;

void playSound(const String sound);
void getTimeCallback(Control *sender, int type);
void syncTimeCallback(Control *sender, int type);
void updateTimeLabel();

TickTwo timeLabelUpdater(updateTimeLabel, 1000, 0, MILLIS);

void setup()
{
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  ESPUI.setVerbosity(Verbosity::Quiet);

  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = true;
  kit.begin(cfg);

  SD.begin(PIN_AUDIO_KIT_SD_CARD_CS, AUDIOKIT_SD_SPI);

  player.setVolume(1.0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP("PIGGY BANK");
  dnsServer.start(DNS_PORT, "*", apIP);

  Wire.setPins(22, 21);
  if (!rtc.begin())
  {
    Serial.println("Could not find RTC module");
  }
  else
  {
    espRTC.setTime(rtc.now().unixtime());
  }

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

  playSound(sounds[HORN]);
}

void loop()
{
  dnsServer.processNextRequest();
  kit.processActions();
  player.copy();
  timeLabelUpdater.update();
}

void playSound(const String sound)
{
  String path = String(SOUND_FOLDER_PATH) + "/" + sound;
  player.setPath(path.c_str());
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
