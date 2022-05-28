/**
 * UDAWA - Universal Digital Agriculture Watering Assistant
 * Firmware for Legacy UDAWA Board
 * Licensed under aGPLv3
 * Researched and developed by PRITA Research Group & Narin Laboratory
 * prita.undiknas.ac.id | narin.co.id
**/
#include "main.h"

Settings mySettings;
uint16_t taskIdPublishWater;
Water water;

const size_t callbacksSize = 13;
GenericCallback callbacks[callbacksSize] = {
  { "sharedAttributesUpdate", processSharedAttributesUpdate },
  { "provisionResponse", processProvisionResponse },
  { "saveConfig", processSaveConfig },
  { "saveSettings", processSaveSettings },
  { "syncClientAttributes", processSyncClientAttributes },
  { "configCoMCUSave", processConfigCoMCUSave },
  { "reboot", processReboot },
  { "setSwitch", processSetSwitch },
  { "getSwitchCh1", processGetSwitch},
  { "getSwitchCh2", processGetSwitch},
  { "getSwitchCh3", processGetSwitch},
  { "getSwitchCh4", processGetSwitch},
  { "syncConfigCoMCU", processSyncConfigCoMCU }
};

void setup()
{
  startup();
  loadSettings();
  syncConfigCoMCU();


  networkInit();
  tb.setBufferSize(DOCSIZE);

  if(mySettings.publishInterval > 0)
  {
    taskIdPublishWater = taskManager.scheduleFixedRate(mySettings.publishInterval, [] {
      publishWater();
    });
  }

  if(mySettings.fTeleDev)
  {
    taskManager.scheduleFixedRate(1000, [] {
      publishDeviceTelemetry();
    });
  }
}

void loop()
{
  udawa();
  dutyRuntime();
  publishSwitch();

  if(tb.connected() && FLAG_IOT_SUBSCRIBE)
  {
    if(tb.callbackSubscribe(callbacks, callbacksSize))
    {
      sprintf_P(logBuff, PSTR("Callbacks subscribed successfuly!"));
      recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
      FLAG_IOT_SUBSCRIBE = false;
    }
    if (tb.Firmware_Update(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION))
    {
      sprintf_P(logBuff, PSTR("OTA Update finished, rebooting..."));
      recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
      reboot();
    }
    else
    {
      sprintf_P(logBuff, PSTR("Firmware up-to-date."));
      recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
    }

    syncClientAttributes();
  }
}

void publishWater()
{
  StaticJsonDocument<DOCSIZE> doc;
  doc["method"] = "getWaterTemp";
  serialWriteToCoMcu(doc, 1);
  if(doc["params"]["celcius"] != nullptr)
  {
    water.celcius = doc["params"]["celcius"];
    tb.sendTelemetryDoc(doc);
  }

  doc.clear();

  doc["method"] = "getWaterEC";
  serialWriteToCoMcu(doc, 1);
  if(doc["params"]["waterEC"] != nullptr)
  {
    water.ec = doc["params"]["waterEC"];
    water.tds = doc["params"]["waterPPM"];
    tb.sendTelemetryDoc(doc);
  }
}

void loadSettings()
{
  StaticJsonDocument<DOCSIZE> doc;
  readSettings(doc, settingsPath);

  if(doc["dc"] != nullptr)
  {
    uint8_t index = 0;
    for(JsonVariant v : doc["dc"].as<JsonArray>())
    {
        mySettings.dc[index] = v.as<uint8_t>();
        index++;
    }
  }
  else
  {
    for(uint8_t i = 0; i < countof(mySettings.dc); i++)
    {
        mySettings.dc[i] = 0;
    }
  }

  if(doc["dr"] != nullptr)
  {
    uint8_t index = 0;
    for(JsonVariant v : doc["dr"].as<JsonArray>())
    {
        mySettings.dr[index] = v.as<unsigned long>();
        index++;
    }
  }
  else
  {
    for(uint8_t i = 0; i < countof(mySettings.dr); i++)
    {
        mySettings.dr[i] = 0;
    }
  }

  if(doc["dcfs"] != nullptr)
  {
    uint8_t index = 0;
    for(JsonVariant v : doc["dcfs"].as<JsonArray>())
    {
        mySettings.dcfs[index] = v.as<uint8_t>();
        index++;
    }
  }
  else
  {
    for(uint8_t i = 0; i < countof(mySettings.dcfs); i++)
    {
        mySettings.dcfs[i] = 0;
    }
  }

  if(doc["drfs"] != nullptr)
  {
    uint8_t index = 0;
    for(JsonVariant v : doc["drfs"].as<JsonArray>())
    {
        mySettings.drfs[index] = v.as<unsigned long>();
        index++;
    }
  }
  else
  {
    for(uint8_t i = 0; i < countof(mySettings.drfs); i++)
    {
        mySettings.drfs[i] = 0;
    }
  }

  if(doc["relayPin"] != nullptr)
  {
    uint8_t index = 0;
    for(JsonVariant v : doc["relayPin"].as<JsonArray>())
    {
        mySettings.relayPin[index] = v.as<uint8_t>();
        index++;
    }
  }
  else
  {
    for(uint8_t i = 0; i < countof(mySettings.relayPin); i++)
    {
        mySettings.relayPin[i] = 0;
    }
  }

  if(doc["ON"] != nullptr)
  {
    mySettings.ON = doc["ON"].as<bool>();
  }
  else
  {
    mySettings.ON = 1;
  }

  if(doc["fTeleDev"] != nullptr)
  {
    mySettings.fTeleDev = (bool)doc["fTeleDev"].as<int>();
  }
  else
  {
    mySettings.fTeleDev = 1;
  }

  for(uint8_t i = 0; i < countof(mySettings.dutyCounter); i++)
  {
    mySettings.dutyCounter[i] = 86400;
  }

  if(doc["publishInterval"] != nullptr)
  {
    mySettings.publishInterval = doc["publishInterval"].as<unsigned long>();
  }
  else
  {
    mySettings.publishInterval = 30000;
  }
}

void saveSettings()
{
  StaticJsonDocument<DOCSIZE> doc;

  JsonArray dc = doc.createNestedArray("dc");
  for(uint8_t i=0; i<countof(mySettings.dc); i++)
  {
    dc.add(mySettings.dc[i]);
  }

  JsonArray dr = doc.createNestedArray("dr");
  for(uint8_t i=0; i<countof(mySettings.dr); i++)
  {
    dr.add(mySettings.dr[i]);
  }

  JsonArray dcfs = doc.createNestedArray("dcfs");
  for(uint8_t i=0; i<countof(mySettings.dcfs); i++)
  {
    dcfs.add(mySettings.dcfs[i]);
  }

  JsonArray drfs = doc.createNestedArray("drfs");
  for(uint8_t i=0; i<countof(mySettings.drfs); i++)
  {
    drfs.add(mySettings.drfs[i]);
  }

  JsonArray relayPin = doc.createNestedArray("relayPin");
  for(uint8_t i=0; i<countof(mySettings.relayPin); i++)
  {
    relayPin.add(mySettings.relayPin[i]);
  }

  doc["ON"] = mySettings.ON;
  doc["fTeleDev"] = mySettings.fTeleDev;

  doc["publishInterval"]  = mySettings.publishInterval;

  writeSettings(doc, settingsPath);
}

callbackResponse processSaveConfig(const callbackData &data)
{
  configSave();
  return callbackResponse("saveConfig", 1);
}

callbackResponse processSaveSettings(const callbackData &data)
{
  saveSettings();
  loadSettings();

  mySettings.lastUpdated = millis();
  return callbackResponse("saveSettings", 1);
}

callbackResponse processConfigCoMCUSave(const callbackData &data)
{
  configCoMCUSave();
  return callbackResponse("configCoMCUSave", 1);
}

callbackResponse processSyncConfigCoMCU(const callbackData &data)
{
  syncConfigCoMCU();
  return callbackResponse("syncConfigCoMCU", 1);
}

callbackResponse processReboot(const callbackData &data)
{
  reboot();
  return callbackResponse("reboot", 1);
}

callbackResponse processSyncClientAttributes(const callbackData &data)
{
  syncClientAttributes();
  return callbackResponse("syncClientAttributes", 1);
}

callbackResponse processSetSwitch(const callbackData &data)
{
  if(data["params"]["ch"] != nullptr && data["params"]["state"] != nullptr)
  {
    String ch = data["params"]["ch"].as<String>();
    String state = data["params"]["state"].as<String>();
    setSwitch(ch, state);
    return callbackResponse(data["params"]["ch"].as<const char*>(), !strcmp((const char*) "ON", data["params"]["state"].as<const char*>()) ? (int)mySettings.ON : (int)!mySettings.ON);
  }
  else
  {
    return callbackResponse("err", "ch not found.");
  }
}

callbackResponse processGetSwitch(const callbackData &data)
{
  const char *methodName = data["method"];
  if(!strcmp((const char*) "getSwitchCh1", methodName))
  {
    return callbackResponse("getSwitchCh1", mySettings.dutyState[0] == mySettings.ON ? 1 : 0);
  }
  else if(!strcmp((const char*) "getSwitchCh2", methodName))
  {
    return callbackResponse("getSwitchCh2", mySettings.dutyState[1] == mySettings.ON ? 1 : 0);
  }
  else if(!strcmp((const char*) "getSwitchCh3", methodName))
  {
    return callbackResponse("getSwitchCh3", mySettings.dutyState[2] == mySettings.ON ? 1 : 0);
  }
  else if(!strcmp((const char*) "getSwitchCh4", methodName))
  {
    return callbackResponse("getSwitchCh4", mySettings.dutyState[3] == mySettings.ON ? 1 : 0);
  }
  return callbackResponse("getSwitch", 0);
}

void setSwitch(String ch, String state)
{
  bool fState = 0;
  uint8_t pin = 0;

  if(ch == String("ch1")){pin = mySettings.relayPin[0]; mySettings.dutyState[0] = (state == String("ON")) ? mySettings.ON : !mySettings.ON; mySettings.publishSwitch[0] = true;}
  else if(ch == String("ch2")){pin = mySettings.relayPin[1]; mySettings.dutyState[1] = (state == String("ON")) ? mySettings.ON : !mySettings.ON; mySettings.publishSwitch[1] = true;}
  else if(ch == String("ch3")){pin = mySettings.relayPin[2]; mySettings.dutyState[2] = (state == String("ON")) ? mySettings.ON : !mySettings.ON; mySettings.publishSwitch[2] = true;}
  else if(ch == String("ch4")){pin = mySettings.relayPin[3]; mySettings.dutyState[3] = (state == String("ON")) ? mySettings.ON : !mySettings.ON; mySettings.publishSwitch[3] = true;}
  if(state == String("ON"))
  {
    fState = mySettings.ON;
  }
  else
  {
    fState = !mySettings.ON;
  }

  setCoMCUPin(pin, 'D', OUTPUT, 0, fState);
}

void dutyRuntime()
{
  for(uint8_t i = 0; i < countof(mySettings.relayPin); i++)
  {
    if(mySettings.dc[i] != 0)
    {
      if( mySettings.dutyState[i] == mySettings.ON )
      {
        if( mySettings.dc[i] != 100 && (millis() - mySettings.dutyCounter[i] ) >= (float)(( ((float)mySettings.dc[i] / 100) * (float)mySettings.dr[i]) * 1000))
        {
          mySettings.dutyState[i] = !mySettings.ON;
          String ch = "ch" + String(i+1);
          setSwitch(ch, "OFF");
          mySettings.dutyCounter[i] = millis();
          sprintf_P(logBuff, PSTR("Relay Ch%d changed to %d - dc:%d - dr:%ld"), i+1, mySettings.dutyState[i], mySettings.dc[i], mySettings.dr[i]);
          recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
        }
      }
      else
      {
        if( mySettings.dc[i] != 0 && (millis() - mySettings.dutyCounter[i] ) >= (float) ( ((100 - (float) mySettings.dc[i]) / 100) * (float) mySettings.dr[i]) * 1000)
        {
          mySettings.dutyState[i] = mySettings.ON;
          String ch = "ch" + String(i+1);
          setSwitch(ch, "ON");
          mySettings.dutyCounter[i] = millis();
          sprintf_P(logBuff, PSTR("Relay Ch%d changed to %d - dc:%d - dr:%ld"), i+1, mySettings.dutyState[i], mySettings.dc[i], mySettings.dr[i]);
          recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
        }
      }
    }
  }
}

callbackResponse processSharedAttributesUpdate(const callbackData &data)
{
  if(config.logLev >= 4){serializeJsonPretty(data, Serial);}

  if(data["model"] != nullptr){strlcpy(config.model, data["model"].as<const char*>(), sizeof(config.model));}
  if(data["group"] != nullptr){strlcpy(config.group, data["group"].as<const char*>(), sizeof(config.group));}
  if(data["broker"] != nullptr){strlcpy(config.broker, data["broker"].as<const char*>(), sizeof(config.broker));}
  if(data["port"] != nullptr){data["port"].as<uint16_t>();}
  if(data["wssid"] != nullptr){strlcpy(config.wssid, data["wssid"].as<const char*>(), sizeof(config.wssid));}
  if(data["wpass"] != nullptr){strlcpy(config.wpass, data["wpass"].as<const char*>(), sizeof(config.wpass));}
  if(data["dssid"] != nullptr){strlcpy(config.dssid, data["dssid"].as<const char*>(), sizeof(config.dssid));}
  if(data["dpass"] != nullptr){strlcpy(config.dpass, data["dpass"].as<const char*>(), sizeof(config.dpass));}
  if(data["upass"] != nullptr){strlcpy(config.upass, data["upass"].as<const char*>(), sizeof(config.upass));}
  if(data["provisionDeviceKey"] != nullptr){strlcpy(config.provisionDeviceKey, data["provisionDeviceKey"].as<const char*>(), sizeof(config.provisionDeviceKey));}
  if(data["provisionDeviceSecret"] != nullptr){strlcpy(config.provisionDeviceSecret, data["provisionDeviceSecret"].as<const char*>(), sizeof(config.provisionDeviceSecret));}
  if(data["logLev"] != nullptr){config.logLev = data["logLev"].as<uint8_t>();}

  if(data["dcCh1"] != nullptr){mySettings.dc[0] = data["dcCh1"].as<uint8_t>();}
  if(data["dcCh2"] != nullptr){mySettings.dc[1] = data["dcCh2"].as<uint8_t>();}
  if(data["dcCh3"] != nullptr){mySettings.dc[2] = data["dcCh3"].as<uint8_t>();}
  if(data["dcCh4"] != nullptr){mySettings.dc[3] = data["dcCh4"].as<uint8_t>();}

  if(data["drCh1"] != nullptr){mySettings.dr[0] = data["drCh1"].as<unsigned long>();}
  if(data["drCh2"] != nullptr){mySettings.dr[1] = data["drCh2"].as<unsigned long>();}
  if(data["drCh3"] != nullptr){mySettings.dr[2] = data["drCh3"].as<unsigned long>();}
  if(data["drCh4"] != nullptr){mySettings.dr[3] = data["drCh4"].as<unsigned long>();}

  if(data["dcfsCh1"] != nullptr){mySettings.dcfs[0] = data["dcfsCh1"].as<uint8_t>();}
  if(data["dcfsCh2"] != nullptr){mySettings.dcfs[1] = data["dcfsCh2"].as<uint8_t>();}
  if(data["dcfsCh3"] != nullptr){mySettings.dcfs[2] = data["dcfsCh3"].as<uint8_t>();}
  if(data["dcfsCh4"] != nullptr){mySettings.dcfs[3] = data["dcfsCh4"].as<uint8_t>();}

  if(data["drfsCh1"] != nullptr){mySettings.drfs[0] = data["drfsCh1"].as<unsigned long>();}
  if(data["drfsCh2"] != nullptr){mySettings.drfs[1] = data["drfsCh2"].as<unsigned long>();}
  if(data["drfsCh3"] != nullptr){mySettings.drfs[2] = data["drfsCh3"].as<unsigned long>();}
  if(data["drfsCh4"] != nullptr){mySettings.drfs[3] = data["drfsCh4"].as<unsigned long>();}

  if(data["relayPinCh1"] != nullptr){mySettings.relayPin[0] = data["relayPinCh1"].as<uint8_t>();}
  if(data["relayPinCh2"] != nullptr){mySettings.relayPin[1] = data["relayPinCh2"].as<uint8_t>();}
  if(data["relayPinCh3"] != nullptr){mySettings.relayPin[2] = data["relayPinCh3"].as<uint8_t>();}
  if(data["relayPinCh4"] != nullptr){mySettings.relayPin[3] = data["relayPinCh4"].as<uint8_t>();}

  if(data["ON"] != nullptr){mySettings.ON = data["ON"].as<bool>();}
  if(data["fTeleDev"] != nullptr){mySettings.fTeleDev = (bool)data["fTeleDev"].as<int>();}
  if(data["publishInterval"] != nullptr){mySettings.publishInterval = data["publishInterval"].as<unsigned long>();}

  if(data["pEcKcoe"] != nullptr){configcomcu.pEcKcoe = data["pEcKcoe"].as<float>();}
  if(data["pEcTcoe"] != nullptr){configcomcu.pEcTcoe = data["pEcTcoe"].as<float>();}
  if(data["pEcVin"] != nullptr){configcomcu.pEcVin = data["pEcVin"].as<float>();}
  if(data["pEcPpm"] != nullptr){configcomcu.pEcPpm = data["pEcPpm"].as<float>();}
  if(data["pEcR1"] != nullptr){configcomcu.pEcR1 = data["pEcR1"].as<uint16_t>();}
  if(data["pEcRa"] != nullptr){configcomcu.pEcRa = data["pEcRa"].as<uint16_t>();}

  if(data["pinEcPower"] != nullptr){configcomcu.pinEcPower = data["pinEcPower"].as<uint8_t>();}
  if(data["pinEcGnd"] != nullptr){configcomcu.pinEcGnd = data["pinEcGnd"].as<uint8_t>();}
  if(data["pinEcData"] != nullptr){configcomcu.pinEcData = data["pinEcData"].as<uint8_t>();}

  mySettings.lastUpdated = millis();
  return callbackResponse("sharedAttributesUpdate", 1);
}

void syncClientAttributes()
{
  StaticJsonDocument<DOCSIZE> doc;

  IPAddress ip = WiFi.localIP();
  char ipa[25];
  sprintf(ipa, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  doc["ipad"] = ipa;
  doc["compdate"] = COMPILED;
  doc["fmTitle"] = CURRENT_FIRMWARE_TITLE;
  doc["fmVersion"] = CURRENT_FIRMWARE_VERSION;
  doc["stamac"] = WiFi.macAddress();
  tb.sendAttributeDoc(doc);
  doc.clear();
  doc["apmac"] = WiFi.softAPmacAddress();
  doc["flashFree"] = ESP.getFreeSketchSpace();
  doc["firmwareSize"] = ESP.getSketchSize();
  doc["flashSize"] = ESP.getFlashChipSize();
  doc["sdkVer"] = ESP.getSdkVersion();
  tb.sendAttributeDoc(doc);
  doc.clear();
  doc["model"] = config.model;
  doc["group"] = config.group;
  doc["broker"] = config.broker;
  doc["port"] = config.port;
  doc["wssid"] = config.wssid;
  tb.sendAttributeDoc(doc);
  doc.clear();
  doc["wpass"] = config.wpass;
  doc["dssid"] = config.dssid;
  doc["dpass"] = config.dpass;
  doc["upass"] = config.upass;
  doc["accessToken"] = config.accessToken;
  doc["provisionDeviceKey"] = config.provisionDeviceKey;
  doc["provisionDeviceSecret"] = config.provisionDeviceSecret;
  doc["logLev"] = config.logLev;
  tb.sendAttributeDoc(doc);
  doc.clear();
  doc["dcCh1"] = mySettings.dc[0];
  doc["dcCh2"] = mySettings.dc[1];
  doc["dcCh3"] = mySettings.dc[2];
  doc["dcCh4"] = mySettings.dc[3];
  doc["drCh1"] = mySettings.dr[0];
  doc["drCh2"] = mySettings.dr[1];
  doc["drCh3"] = mySettings.dr[2];
  doc["drCh4"] = mySettings.dr[3];
  tb.sendAttributeDoc(doc);
  doc.clear();
  doc["dcfsCh1"] = mySettings.dcfs[0];
  doc["dcfsCh2"] = mySettings.dcfs[1];
  doc["dcfsCh3"] = mySettings.dcfs[2];
  doc["dcfsCh4"] = mySettings.dcfs[3];
  doc["drfsCh1"] = mySettings.drfs[0];
  doc["drfsCh2"] = mySettings.drfs[1];
  doc["drfsCh3"] = mySettings.drfs[2];
  doc["drfsCh4"] = mySettings.drfs[3];
  tb.sendAttributeDoc(doc);
  doc.clear();
  doc["relayPinCh1"] = mySettings.relayPin[0];
  doc["relayPinCh2"] = mySettings.relayPin[1];
  doc["relayPinCh3"] = mySettings.relayPin[2];
  doc["relayPinCh4"] = mySettings.relayPin[3];
  doc["ON"] = mySettings.ON;
  doc["fTeleDev"] = mySettings.fTeleDev;
  doc["publishInterval"] = mySettings.publishInterval;
  tb.sendAttributeDoc(doc);
  doc.clear();
}

void publishDeviceTelemetry()
{
  StaticJsonDocument<DOCSIZE> doc;

  doc["heap"] = heap_caps_get_free_size(MALLOC_CAP_8BIT);;
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis()/1000;
  tb.sendTelemetryDoc(doc);
  doc.clear();
}

void publishSwitch(){
  if(tb.connected()){
    for (uint8_t i = 0; i < 4; i++){
      if(mySettings.publishSwitch[i]){
        StaticJsonDocument<DOCSIZE> doc;
        String chName = "ch" + String(i+1);
        doc[chName.c_str()] = (int)mySettings.dutyState[i];
        tb.sendTelemetryDoc(doc);
        doc.clear();

        mySettings.publishSwitch[i] = false;
      }
    }
  }
}