#include <HTTPClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <FS.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>
#include <RF24.h>
#include <RF24_config.h>
#include <nRF24L01.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
// #include <WiFiClient.h>
// #include <WebServer.h>
WebServer server(80);

int lcdColumns = 20;
int lcdRows = 4;

#include <EEPROM.h>

#include "pgmspace.h"
#define JSON_CONFIG_FILE "/test_config.json"
bool shouldSaveConfig = false;
char testString[50] = "test value";
int testNumber = 1234;

WiFiManager wm;



#define LINES 4       // количество строк дисплея
#define SETTINGS_AMOUNT 7  // количество настроек
#define FAST_STEP 5   // скорость изменения при быстром повороте
char msg[6] = "hello";
RF24 radio(12, 14, 26, 25, 27);

TaskHandle_t Display;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
const int TempKotel = 19;
const int TempStreet = 18;
OneWire tempKot(TempKotel);
OneWire tempStr(TempStreet);

DallasTemperature tempKotel(&tempKot);
DallasTemperature tempStreet(&tempStr);
// передать ссылку на oneWire библиотеке DallasTemperature
// DallasTemperature sensors(&oneWire);

// переменная для хранения адресов устройств
DeviceAddress Thermometer;

int deviceCount = 0;

const char* serverName = "http://10.253.253.54/post-data.php";
String apiKeyValue = "tPmAT5Ab3j7F9";

// пины энкодера
#define CLK 32
#define DT 33
#define SW 5

#include "GyverEncoder.h"
Encoder enc1(CLK, DT, SW);  // для работы c кнопкой

// #include <LiquidCrystal_I2C.h>
// LiquidCrystal_I2C lcd(0x27, 16, 2); // адрес 0x27 или 0x3f
int8_t arrowPos = 0;
int8_t screenPos = 0; // номер "экрана"
int vals[SETTINGS_AMOUNT];
bool isFirstRun;

const char* ssid = "Komfort";
const char* password = "123456789";
char htmlResponse[3000];
struct WiFiSet{
  // char ssid[32];
  String ssid[32];
  String pwd[32];
  // char pwd[32];
};

WiFiSet wifisetting;

//Массив пунктов меню который выводится на экране
const char *settingsNames[]  = {
  "MaxT kotla",
  "MaxT home",
  "Ts1MIN",
  "Ts2MIN",
  "Ts3MIN",
  "ModeAuto",
  "WiFi",
};
//Структура хранения данных в EEptrom, рассчитана на 6 пунктов меню 5 из которых имеют тип данных int, 1 пункт тип данных bool
struct MenuItem{
  char name[10];
  int value;
  bool mode;
  };
  
  MenuItem param[7];

  byte customWifiOn[]={0b01110, 0b10001, 0b01110, 0b10001, 0b00100, 0b01010, 0b00100, 0b00100};
  byte customWifiOff[]={0b00000, 0b00000, 0b00000, 0b10001, 0b01010, 0b00100, 0b01110, 0b10101};


void setup(){
  Serial.begin(115200);
  enc1.setType(TYPE2);
  EEPROM.begin(500);
    EEPROM.get(0,param);
    EEPROM.commit();
  lcd.init();
  lcd.clear();
  // lcd.backlight();
  Wire.begin();
  radio.begin();
  radio.setChannel(5);

  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.openReadingPipe(1, 0x7878787878LL);
  radio.powerUp();
  radio.startListening();
  
  pinMode(15, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);
  //Читаем данные из EEPROM
  for(int i=0; i<7;i++){
    EEPROM.get(i*sizeof(MenuItem), param[i]);
    EEPROM.commit();
  }

  // WiFi.begin("vasal","12345678");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.println("Connecting to WiFi..");
  // }
 
  // Serial.println("Connected to the WiFi network");
  lcd.createChar(0,customWifiOn);
  lcd.createChar(1,customWifiOff);

  }


//Заполнение структры массива данными по умолчанию
void writeParamStruct(){
  sprintf(param[0].name, "TempKMax");
  param[0].value = 80;
  
  sprintf(param[1].name, "TempHMax");
  param[1].value = 25;
  
  sprintf(param[2].name, "TS1MIN");
  param[2].value = 10;
  
  sprintf(param[3].name, "TS2MIN");
  param[3].value = -5;
  
  sprintf(param[4].name, "TS3MIN");
  param[4].value = -15;
  
  sprintf(param[5].name, "AutoMode");
  param[5].mode = true;

  sprintf(param[6].name, "WiFi");
  param[6].mode = true;
  
  for (int i = 0; i < 7; i++) {
    int address = i * sizeof(struct MenuItem);
    EEPROM.put(address, param[i]);
    EEPROM.commit();
  }
    
}  

//Вывод пунктов меню с данными на дисплей 1602
void displayMenu() {
  lcd.clear();
  screenPos = arrowPos / LINES; 
  // Отображение текущего пункта меню и его значение на первой строке дисплеяfals

  for (int i = 0; i < LINES; i++) {
    lcd.setCursor(0, i);
    if (arrowPos == LINES * screenPos + i) lcd.write(126);  // рисуем стрелку
    else lcd.write(32);     // рисуем пробел

    // если пункты меню закончились, покидаем цикл for
    if (LINES * screenPos + i == SETTINGS_AMOUNT) break;
    lcd.print(settingsNames[LINES * screenPos+i]);
    lcd.setCursor(12, i);
    if(settingsNames[LINES * screenPos+i]== "ModeAuto"){
      lcd.print(param[LINES * screenPos+i].mode ? "true":"false");
    }else if(settingsNames[LINES * screenPos+i]== "WiFi"){
      lcd.print(param[LINES * screenPos+i].mode ? "on":"off");
    }else{
    lcd.print(param[LINES * screenPos+i].value);
    }
  }
}

void loop(){
  
  if(param[6].mode==1){
    if(WiFi.status() != WL_CONNECTED){
      WiFi.begin("vasal","12345678");
    }
  }else{
    WiFi.mode(WIFI_OFF);
  }
 
    static uint32_t tmr, tmrhttp, tmrdis;
    static uint32_t tmrBacklight;
    if(millis()-tmrBacklight >=30000){
      tmrBacklight=millis();
      lcd.noBacklight();
    }
  enc1.tick();
  //Если было двойное нажатие то записываем данные по умолчанию
  if(enc1.isClick()){
    lcd.backlight();  
  }

  if(enc1.isDouble()){
    writeParamStruct();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Default");
    delay(5000);
    displayMenu();
  }

  //Запись данных при удержании нажатия энкодера
  if(enc1.isHolded()){
    
    for(int i = 0; i < 7;i++){
      int address = i * sizeof(struct MenuItem); //Получаем адресс выбранной ячейки
      EEPROM.put(address,param[i]); //Записывавем по актуальные данные по выбранному меню
    }
    EEPROM.commit();

    lcd.clear();
    

    lcd.setCursor(0,0);
    lcd.print("Write");
    delay(5000);
    displayMenu();
   } 
   if (enc1.isTurn()){
    //Блок выбора пункта меню
    lcd.backlight();

    int increment =0;                          
    if(enc1.isRight()) increment =1;
    if(enc1.isLeft()) increment =-1;
    arrowPos += increment;
    arrowPos = constrain(arrowPos,0,  SETTINGS_AMOUNT -1);

    //Блок изменения значения пункта меню при удержании нажатия и прокручивании энкодера.
    increment = 0;  // обнуляем инкремент
    if(enc1.isRightH()){
      switch(arrowPos){
        case 0: param[arrowPos].value += 5; break;
        case 1: param[arrowPos].value += 1; break;
        case 2: param[arrowPos].value += 1; break;
        case 3: param[arrowPos].value += 1; break;
        case 4: param[arrowPos].value += 1; break;
        case 5: param[arrowPos].mode =!param[arrowPos].mode; break;
        case 6: param[arrowPos].mode =!param[arrowPos].mode; break;
      }
    }
    if(enc1.isLeftH()){
      switch(arrowPos){
        case 0: param[arrowPos].value -= 5; break;
        case 1: param[arrowPos].value -= 1; break;
        case 2: param[arrowPos].value -= 1; break;
        case 3: param[arrowPos].value -= 1; break;
        case 4: param[arrowPos].value -= 1; break;
        case 5: param[arrowPos].mode = !param[arrowPos].mode; break;
        case 6: param[arrowPos].mode = !param[arrowPos].mode; break;
      }
    }
    
      displayMenu();
    
  }else{  

    if(millis()-tmr >=10000){
      tmr=millis();
   float tempH, temp1, temp2, temp3 = 0;
   tempKotel.requestTemperatures();
   float tempK = tempKotel.getTempCByIndex(0);   
   float tempS = tempStreet.getTempCByIndex(0);   
   
  
  if(radio.available()){
    radio.read(&tempH, sizeof(tempH));
    // Serial.print("Температура: ");
    // Serial.println(tempH);
  } else{
    tempH = 0;
    Serial.println("Не удалось получить данные.");
  }
  lcd.setCursor(0,0);
  updateLcdTempSt(temp1, temp2, temp3, tempH, tempS, tempK);

  tenControl(tempH, tempK, tempS);
  }
  }
  //Отправка данных на сервер
  if(millis()-tmrhttp > 300000){
    tmrhttp=millis();
  HTTPClient http;

  http.begin(serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  float tempH=0;
  radio.read(&tempH, sizeof(tempH));

  tempKotel.requestTemperatures();
   float tempK = tempKotel.getTempCByIndex(0);   
   float tempS = tempStreet.getTempCByIndex(0); 
  //  boolean ten1 =digitalRead()

  String httpRequestData = "api_key=" + apiKeyValue + "&tempH=" + tempH + "&tempS=" + tempS + "&tempK=" + tempK + "&ten1=" + digitalRead(15) + "&ten2=" + digitalRead(2) + "&ten3=" + digitalRead(4) + "&pressure="+ "0" + "";
 
Serial.print("httpRequestData: ");
 
Serial.println(httpRequestData);

int httpResponseCode = http.POST(httpRequestData);
if (httpResponseCode>0) { 
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode); 
}else {
  Serial.print("Error code: ");
  Serial.println(httpResponseCode); 
} 
// Освобождаем память
http.end();
  }

}

  void updateLcdTempSt(float temp1, float temp2, float temp3, float tempH, float tempS, float tempK){
  lcd.clear();
  lcd.setCursor(0,0); 
  if(temp1 != tempS){
  lcd.print("Ts: "); lcd.print(tempS,0);
  temp1 = tempS;  
  }else{
    lcd.print("Ts: "); lcd.print(tempS,0);   
  }

  lcd.setCursor(0,1);  
  if(temp2 != tempH){
  lcd.print("Th: "); lcd.print(tempH,0);
  temp2 = tempH;  
  }else{
    lcd.print("Th: "); lcd.print(tempH,0);    
  }
  
  lcd.setCursor(0,2);  
  if(temp3!= tempK){
  lcd.print("Tk: "); lcd.print(tempK,0);
  temp3 = tempK;  
  }else{
    lcd.print("Tk: "); lcd.print(tempK,0);
  }

  lcd.setCursor(0,3);  
  // bool cm = "1";
  if(param[5].mode==0){
    lcd.print("CM: "); lcd.print("MANUAL");
  }else{
    lcd.print("CM: "); lcd.print("AUTO");
  }

  lcd.setCursor(19,0);
  // if(param[6].mode==0){
  if(WiFi.status() != WL_CONNECTED){
    lcd.write(1);
  }else{
    lcd.write(0);    
  }

  lcd.setCursor(9,0);
  if(digitalRead(15)==1){
    lcd.print("TEN1: ");lcd.print("ON");
  }else{
    lcd.print("TEN1: ");lcd.print("OFF");
  }

  lcd.setCursor(9,1);
  if(digitalRead(2)==1){
    lcd.print("TEN2: ");lcd.print("ON");
  }else{
    lcd.print("TEN2: ");lcd.print("OFF");
  }

  lcd.setCursor(9,2);
  if(digitalRead(4)==1){
    lcd.print("TEN3: ");lcd.print("ON");
  }else{
    lcd.print("TEN3: ");lcd.print("OFF");
  }  
}

void tenControl(float tempH, float tempK, float tempS){
   int ten[]={15};

  int i=0 ;
  unsigned long timing;
  
  if (tempS<=param[3].value){
  // (tempS <=15){
      // int ten[] = {15};
  // }else if(tempS<=-5){
      int ten[] = {15,2}; 
  }else if(tempS <= param[4].value){
      int ten[] = {15,2,4};
  }
  
  if (tempH <=param[1].value){        
    int i,y=0;

    if(millis()-timing > 15000){
      // if(i>2){
    for(i=0; i<3; i++){
      
       if( tempK < param[0].value){
        // if(tempK < 80){
         digitalWrite(ten[i], HIGH);
      }else if(tempK > param[0].value){
          digitalWrite(ten[i],LOW);                    
         }        
      // i++;  
      } 
      timing = millis();  
      
      }
      //  }  
    // }    
   }else{
      for(i=0; i<3 ;i++){
        // if(millis()-timing > 15000){
        // if(digitalRead(ten[i])==HIGH && tempH > 25){
          // if(tempK > 90){
            digitalWrite(ten[i], LOW);
          // }else if(tempK < 80) {
            // digitalWrite(ten[i], HIGH);
            // }
      // }
      // timing = millis(); 
      // }
      
    }    
  }
  }  