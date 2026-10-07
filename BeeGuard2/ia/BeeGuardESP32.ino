#include <driver/i2s.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "BeeGuardFeatures.h"
#include "classifier_model.h"

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 26
#define SAMPLE_RATE 16000
#define CAPTURE_SAMPLES 160000UL
#define I2S_BLOCK 256

const char *WIFI_SSID="CONFIGURE_WIFI_NAME";
const char *WIFI_PASSWORD="CONFIGURE_WIFI_PASSWORD";
// Use the server computer's LAN IP, e.g. http://192.168.1.20:8000/api/classification
const char *API_URL="http://192.168.1.10:8000/api/classification";
const char *DEVICE_ID="beeguard-01";
BeeGuardFeatures extractor;
int32_t i2sWords[I2S_BLOCK];
double features[BeeGuardModel::FEATURE_COUNT];

bool setupI2S(){i2s_config_t c={.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX),.sample_rate=SAMPLE_RATE,.bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT,.channel_format=I2S_CHANNEL_FMT_ONLY_LEFT,.communication_format=I2S_COMM_FORMAT_I2S,.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1,.dma_buf_count=8,.dma_buf_len=256,.use_apll=false,.tx_desc_auto_clear=false,.fixed_mclk=0};i2s_pin_config_t p={.bck_io_num=I2S_SCK,.ws_io_num=I2S_WS,.data_out_num=I2S_PIN_NO_CHANGE,.data_in_num=I2S_SD};if(i2s_driver_install(I2S_NUM_0,&c,0,NULL)!=ESP_OK)return false;return i2s_set_pin(I2S_NUM_0,&p)==ESP_OK;}
bool capture(){extractor.reset();uint32_t total=0;while(total<CAPTURE_SAMPLES){uint32_t want=min((uint32_t)I2S_BLOCK,CAPTURE_SAMPLES-total);size_t bytes=0;if(i2s_read(I2S_NUM_0,i2sWords,want*sizeof(int32_t),&bytes,portMAX_DELAY)!=ESP_OK)return false;uint32_t got=bytes/sizeof(int32_t);if(!got)return false;for(uint32_t i=0;i<got&&total<CAPTURE_SAMPLES;++i){if(!extractor.addSample((int16_t)(i2sWords[i]>>16)))return false;++total;}}return extractor.finish(features);}
void postResult(int cls,float confidence){if(WiFi.status()!=WL_CONNECTED)return;WiFiClient client;HTTPClient http;if(!http.begin(client,API_URL))return;http.addHeader("Content-Type","application/json");char body[640];snprintf(body,sizeof(body),"{\"classification\":\"%s\",\"confidence\":%.6f,\"device_id\":\"%s\",\"dominant_frequency\":%.2f,\"db_mean\":%.3f,\"db_max\":%.3f,\"rms\":%.6f,\"energy_100_300\":%.9f,\"energy_200_270\":%.9f,\"energy_300_600\":%.9f}",BeeGuardModel::CLASS_NAMES[cls],confidence,DEVICE_ID,features[8],features[4],features[5],features[0],features[13],features[35],features[15]);int code=http.POST((uint8_t*)body,strlen(body));Serial.printf("API HTTP %d\n",code);http.end();}
void setup(){Serial.begin(115200);if(!setupI2S()){Serial.println("Falha ao iniciar I2S");while(true)delay(1000);}WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print('.');}Serial.println("\nBeeGuard pronto: captura local de 10 s, sem WAV.");}
void loop(){Serial.println("Analisando 10 s...");if(!capture()){Serial.println("Erro de captura");delay(1000);return;}float feature32[BeeGuardModel::FEATURE_COUNT];for(size_t i=0;i<BeeGuardModel::FEATURE_COUNT;++i)feature32[i]=(float)features[i];float confidence=0;int cls=BeeGuardModel::predict(feature32,&confidence);Serial.printf("%s %.1f%% | dom %.1f Hz | dB %.1f | RMS %.5f | E200-270 %.8f\n",BeeGuardModel::CLASS_NAMES[cls],confidence*100,features[8],features[4],features[0],features[35]);postResult(cls,confidence);delay(1000);}
