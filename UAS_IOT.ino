#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>
#include <PubSubClient.h>

#define WIFI_SSID "Was"
#define WIFI_PASSWORD ""

#define BOT_TOKEN "8739672552:AAGl23e_4gSn1FUFrFfZ56f5bl-P7VWr8HU"
#define CHAT_ID "-1003576559597"

// konfigurasi kredensial antares sesuai log
const char* mqtt_server = "mqtt.antares.id";
const int mqtt_port = 1883;
const char* antares_access_key = "24d5f33b87a5cfdf:91b3cb47f9ff7a4e";
const char* antares_project = "SWMM";
const char* antares_device = "SWMMDevice";

#define MQ2_PIN 32        
#define TRIG_EXT 17       
#define ECHO_EXT 18       
#define TRIG_INT 19       
#define ECHO_INT 21       
#define DHT_PIN 16        
#define DHT_TYPE DHT11
#define SERVO_PIN 25      

#define SERVO_OPEN_ANGLE 90
#define SERVO_CLOSE_ANGLE 0

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

WiFiClient mqtt_client;
PubSubClient mqtt(mqtt_client);

Servo lidServo;
DHT dht(DHT_PIN, DHT_TYPE);

#define Board ("ESP-32")
#define Type ("MQ-2")
#define Voltage_Resolution (3.3)
#define ADC_Bit_Resolution (12) 
#define RatioMQ2CleanAir (9.83) 
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ2_PIN, Type);

long lastSensorRead = 0;
long lastProximityRead = 0; 
long lastTelegramPoll = 0;
long lastSerialPrint = 0;
long lastMqttPublish = 0;
long lastMqttReconnect = 0;
long lidOpenMillis = 0;

const long SENSOR_INTERVAL = 2000;      
const long ULTRASONIC_INTERVAL = 150;   
const long TELEGRAM_INTERVAL = 3000;    
const long SERIAL_INTERVAL = 5000;      
const long MQTT_INTERVAL = 5000;        
const long LID_OPEN_DURATION = 15000;   

bool isLidOpen = false;
int binHeightCm = 21; 

bool tempAlertSent = false;
bool humAlertSent = false;
bool gasAlertSent = false;
bool fullAlertSent = false;

float temperature = 0;
float humidity = 0;
float gasPPM = 0;
float fillPercentage = 0;
float currentDistanceInt = 0; 

float readDistance(int trigPin, int echoPin);
void handleNewMessages(int numNewMessages);
void processSensors();
void processProximity(); 
void evaluateThresholds();
void printToSerial();
void handleSerialCommands();
bool reconnectMqtt();
void mqttCallback(char* topic, byte* payload, int length);

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_EXT, OUTPUT);
  pinMode(ECHO_EXT, INPUT);
  pinMode(TRIG_INT, OUTPUT);
  pinMode(ECHO_INT, INPUT);
  
  lidServo.setPeriodHertz(50);
  lidServo.attach(SERVO_PIN, 500, 2400); 
  lidServo.write(SERVO_CLOSE_ANGLE); 
  
  dht.begin();
  
  MQ2.setRegressionMethod(1); 
  MQ2.setA(3616.1); MQ2.setB(-2.675); 
  MQ2.init();
  
  Serial.print("menyiapkan sensor gas mq2...");
  float calcR0 = 0;
  for(int i = 1; i <= 10; i ++) {
    MQ2.update(); 
    calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
  }
  MQ2.setR0(calcR0 / 10);
  Serial.println(" kalibrasi internal selesai.");
  if(isinf(calcR0)) { Serial.println("peringatan sistem: mq2 gagal merespon."); }

  Serial.print("menghubungkan perangkat ke wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nberhasil terhubung ke jaringan internet.");

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  
  // wajib untuk antares agar payload yang besar tidak terpotong
  mqtt.setBufferSize(1024);
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  long currentMillis = millis();

  if (!mqtt.connected()) {
    if (currentMillis - lastMqttReconnect > 5000) {
      lastMqttReconnect = currentMillis;
      if (reconnectMqtt()) {
        lastMqttReconnect = 0;
      }
    }
  } else {
    mqtt.loop();
  }
  
  if (currentMillis - lastProximityRead >= ULTRASONIC_INTERVAL) {
    lastProximityRead = currentMillis;
    processProximity();
  }

  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    processSensors();
    evaluateThresholds(); 
  }
  
  if (currentMillis - lastMqttPublish >= MQTT_INTERVAL) {
    lastMqttPublish = currentMillis;
    if (mqtt.connected()) {
      // serialisasi data sensor ke objek json
      StaticJsonDocument<200> innerDoc;
      innerDoc["suhu"] = temperature;
      innerDoc["kelembaban"] = humidity;
      innerDoc["gas"] = gasPPM;
      innerDoc["kapasitas"] = fillPercentage;
      String innerJson;
      serializeJson(innerDoc, innerJson);

      // mengubah tanda kutip agar sesuai dengan format string bersarang onem2m
      innerJson.replace("\"", "\\\""); 
      
      // merakit manual payload string agar urutan format onem2m persis dengan standar antares
      String payload = "{\"m2m:rqp\":{";
      payload += "\"fr\":\"" + String(antares_access_key) + "\",";
      payload += "\"to\":\"/antares-cse/antares-id/" + String(antares_project) + "/" + String(antares_device) + "\",";
      payload += "\"op\":1,";
      payload += "\"rqi\":\"123456\","; 
      payload += "\"pc\":{\"m2m:cin\":{\"cnf\":\"message\",\"con\":\"" + innerJson + "\"}},"; 
      payload += "\"ty\":4";
      payload += "}}";

      String pubTopic = "/oneM2M/req/" + String(antares_access_key) + "/antares-cse/json";
      bool terkirim = mqtt.publish(pubTopic.c_str(), payload.c_str());

      Serial.println("\n--- diagnostik transmisi antares ---");
      Serial.print("ukuran paket: "); Serial.print(payload.length()); Serial.println(" byte");
      Serial.println("paket data  : " + payload);
      
      if (terkirim) {
        Serial.println("status      : berhasil dikirim ke broker antares.");
      } else {
        Serial.println("status      : gagal dikirim! pastikan koneksi stabil.");
      }
      Serial.println("------------------------------------\n");
    }
  }

  if (currentMillis - lastTelegramPoll >= TELEGRAM_INTERVAL) {
    lastTelegramPoll = currentMillis;
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }

  handleSerialCommands();

  if (currentMillis - lastSerialPrint >= SERIAL_INTERVAL) {
    lastSerialPrint = currentMillis;
    printToSerial();
  }
  
  if (isLidOpen && (millis() - lidOpenMillis >= LID_OPEN_DURATION)) {
    lidServo.write(SERVO_CLOSE_ANGLE);
    isLidOpen = false;
    Serial.println("sistem: durasi habis, penutup swmm ditutup otomatis.");
  }
}

bool reconnectMqtt() {
  Serial.print("mencoba terhubung ke broker mqtt antares...");
  String clientId = "swmmclient-" + String(random(0xffff), HEX);
  
  if (mqtt.connect(clientId.c_str())) {
    Serial.println("berhasil.");
    String subTopic = "/oneM2M/req/antares-cse/" + String(antares_access_key) + "/json";
    mqtt.subscribe(subTopic.c_str());
    return true;
  } else {
    Serial.println("gagal, akan mencoba lagi nanti.");
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (message.indexOf("\"con\":\"1\"") > 0 || message.indexOf("\"con\":\"open\"") > 0) {
    lidServo.write(SERVO_OPEN_ANGLE);
    isLidOpen = true;
    lidOpenMillis = millis();
    Serial.println("mqtt antares: penutup dibuka dari jarak jauh.");
  } else if (message.indexOf("\"con\":\"0\"") > 0 || message.indexOf("\"con\":\"close\"") > 0) {
    lidServo.write(SERVO_CLOSE_ANGLE);
    isLidOpen = false;
    Serial.println("mqtt antares: penutup ditutup dari jarak jauh.");
  }
}

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000); 
  if (duration == 0) return -1; 
  
  return (duration * 0.0343) / 2.0; 
}

void processProximity() {
  float distExt = readDistance(TRIG_EXT, ECHO_EXT);
  
  if (distExt > 0 && distExt <= 10.0) { 
    if (!isLidOpen) { 
      Serial.println("sensor luar: pergerakan terdeteksi, membuka penutup swmm.");
    }
    lidServo.write(SERVO_OPEN_ANGLE); 
    isLidOpen = true;
    lidOpenMillis = millis(); 
  }
}

void processSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  }

  MQ2.update(); 
  gasPPM = MQ2.readSensor();

  currentDistanceInt = readDistance(TRIG_INT, ECHO_INT);

  if (currentDistanceInt > 0) {
    float fill = ((binHeightCm - currentDistanceInt) / binHeightCm) * 100.0;
    fillPercentage = constrain(fill, 0, 100); 
  }
}

void evaluateThresholds() {
  if (temperature >= 38.0 && !tempAlertSent) {
    String alertMsg = "*!!! PERINGATAN SWMM !!!*\n";
    alertMsg += "-----------------------\n";
    alertMsg += "*Kondisi:* SUHU TINGGI\n";
    alertMsg += "*Terbaca:* " + String(temperature) + " °C\n\n";
    alertMsg += "Mohon segera periksa perangkat untuk menghindari risiko panas berlebih.";
    bot.sendMessage(CHAT_ID, alertMsg, "Markdown");
    Serial.println("peringatan: suhu swmm terdeteksi tinggi.");
    tempAlertSent = true;
  } else if (temperature < 35.0) { tempAlertSent = false; }

  if (humidity > 60.0 && !humAlertSent) {
    String alertMsg = "*!!! PERINGATAN SWMM !!!*\n";
    alertMsg += "-----------------------\n";
    alertMsg += "*Kondisi:* KELEMBABAN TINGGI\n";
    alertMsg += "*Terbaca:* " + String(humidity) + " %\n\n";
    alertMsg += "Sirkulasi udara mungkin terhambat, berpotensi mempercepat pembusukan.";
    bot.sendMessage(CHAT_ID, alertMsg, "Markdown");
    Serial.println("peringatan: kelembaban swmm terdeteksi tinggi.");
    humAlertSent = true;
  } else if (humidity < 55.0) { humAlertSent = false; }

  if (gasPPM >= 600.0 && !gasAlertSent) {
    String alertMsg = "*!!! PERINGATAN SWMM !!!*\n";
    alertMsg += "-----------------------\n";
    alertMsg += "*Kondisi:* GAS/BAU MENYENGAT\n";
    alertMsg += "*Terbaca:* " + String(gasPPM) + " ppm\n\n";
    alertMsg += "Terdeteksi penumpukan gas amonia atau metana dari proses pembusukan.";
    bot.sendMessage(CHAT_ID, alertMsg, "Markdown");
    Serial.println("peringatan: tingkat gas swmm tinggi.");
    gasAlertSent = true;
  } else if (gasPPM < 400.0) { gasAlertSent = false; }

  if (fillPercentage >= 80.0 && !fullAlertSent) {
    String alertMsg = "*!!! PERINGATAN SWMM !!!*\n";
    alertMsg += "-----------------------\n";
    alertMsg += "*Kondisi:* KAPASITAS HAMPIR PENUH\n";
    alertMsg += "*Terbaca:* " + String(fillPercentage) + " % (Sisa jarak: " + String(currentDistanceInt) + " cm)\n\n";
    alertMsg += "Mohon jadwalkan pengangkutan sampah segera.";
    bot.sendMessage(CHAT_ID, alertMsg, "Markdown");
    Serial.println("peringatan: tingkat penuh swmm hampir maksimal.");
    fullAlertSent = true;
  } else if (fillPercentage < 70.0) { fullAlertSent = false; }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    if (chat_id != CHAT_ID) continue; 

    if (text == "/start" || text == "/help" || text == "/start@swmmIoT_bot" || text == "/help@swmmIoT_bot") {
      String helpMsg = "*[ SISTEM PENGONTROL SWMM ]*\n";
      helpMsg += "--------------------------\n";
      helpMsg += "Halo, saya adalah asisten bot untuk fasilitas tempat sampah cerdas Anda.\n\n";
      helpMsg += "*Daftar Perintah:*\n";
      helpMsg += "*/start* atau */help*\nMenampilkan daftar panduan ini.\n\n";
      helpMsg += "*/open*\nMembuka penutup bak secara paksa.\n\n";
      helpMsg += "*/close*\nMenutup penutup bak secara manual.\n\n";
      helpMsg += "*/status*\nMencetak laporan lengkap diagnostik sensor saat ini.";
      
      bot.sendMessage(chat_id, helpMsg, "Markdown");
      Serial.println("telegram: mengirim daftar perintah.");
    }
    else if (text == "/open" || text == "/open@swmmIoT_bot") {
      lidServo.write(SERVO_OPEN_ANGLE);
      isLidOpen = true;
      lidOpenMillis = millis();
      String msg = "*[ KONTROL SWMM ]*\n>> Status: *PENUTUP TERBUKA*\n\nInstruksi berhasil dieksekusi.";
      bot.sendMessage(chat_id, msg, "Markdown");
      Serial.println("telegram: memproses perintah buka penutup swmm.");
    } 
    else if (text == "/close" || text == "/close@swmmIoT_bot") {
      lidServo.write(SERVO_CLOSE_ANGLE);
      isLidOpen = false;
      String msg = "*[ KONTROL SWMM ]*\n>> Status: *PENUTUP TERTUTUP*\n\nInstruksi berhasil dieksekusi.";
      bot.sendMessage(chat_id, msg, "Markdown");
      Serial.println("telegram: memproses perintah tutup penutup swmm.");
    } 
    else if (text == "/status" || text == "/status@swmmIoT_bot") {
      String stat = "*[ LAPORAN STATUS SWMM ]*\n";
      stat += "--------------------------\n";
      stat += "*KAPASITAS RUANG*\n";
      stat += "- Tingkat Penuh : " + String(fillPercentage) + " %\n";
      stat += "- Jarak Aktual  : " + String(currentDistanceInt) + " cm\n\n";
      stat += "*KONDISI LINGKUNGAN*\n";
      stat += "- Suhu Udara    : " + String(temperature) + " °C\n";
      stat += "- Kelembaban    : " + String(humidity) + " %\n";
      stat += "- Tingkat Gas   : " + String(gasPPM) + " ppm\n\n";
      stat += "*STATUS PERANGKAT*\n";
      stat += "- Posisi Penutup: *" + String(isLidOpen ? "TERBUKA" : "TERTUTUP") + "*\n";
      stat += "--------------------------";
      
      bot.sendMessage(chat_id, stat, "Markdown");
      Serial.println("telegram: melayani permintaan status swmm.");
    }
  }
}

void printToSerial() {
  Serial.println("");
  Serial.println("status swmm saat ini");
  Serial.print("jarak aktual  : "); Serial.print(currentDistanceInt); Serial.println(" cm");
  Serial.print("tingkat penuh : "); Serial.print(fillPercentage); Serial.println(" %");
  Serial.print("suhu          : "); Serial.print(temperature); Serial.println(" c");
  Serial.print("kelembaban    : "); Serial.print(humidity); Serial.println(" %");
  Serial.print("tingkat gas   : "); Serial.print(gasPPM); Serial.println(" ppm");
  Serial.print("status penutup: "); Serial.println(isLidOpen ? "terbuka" : "tertutup");
  Serial.println("");
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); 
    command.toLowerCase(); 

    if (command == "start" || command == "/start" || command == "help" || command == "/help") {
      Serial.println("daftar perintah swmm:");
      Serial.println("start atau /start - sapaan, tampilkan daftar perintah");
      Serial.println("help atau /help - tampilkan daftar perintah");
      Serial.println("open atau /open - buka penutup swmm");
      Serial.println("close atau /close - tutup penutup swmm");
      Serial.println("status atau /status - lihat status swmm (suhu, kelembaban, tingkat gas, tingkat penuh)");
    }
    else if (command == "open" || command == "/open") {
      lidServo.write(SERVO_OPEN_ANGLE);
      isLidOpen = true;
      lidOpenMillis = millis();
      Serial.println("serial: instruksi buka penutup swmm dijalankan.");
    } 
    else if (command == "close" || command == "/close") {
      lidServo.write(SERVO_CLOSE_ANGLE);
      isLidOpen = false;
      Serial.println("serial: instruksi tutup penutup swmm dijalankan.");
    } 
    else if (command == "status" || command == "/status") {
      Serial.println("serial: memanggil status swmm.");
      printToSerial();
    }
  }
}