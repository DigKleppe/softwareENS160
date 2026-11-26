
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "guiTask.h"
#include "i2c.h"

#include "CGIcommonScripts.h"
#include "averager.h"
#include "cgiScripts.h"
#include "sensorTask.h"
#include "wifiConnect.h"

#include "time.h"

#include <esp_err.h>
#include <esp_log.h>

#define TIMEOUT 50 // * 100ms

#define DAYLOGINTERVAL 5   // minutes
#define HOURLOGINTERVAL 10 // seconds

#define AVERAGES1 1									  // number of values to average first
#define AVERAGES2 ((DAYLOGINTERVAL * 60) / AVERAGES1) // number of values to average secondary for daylog
#define SKIPFIRSTSAMPLES 4;							  // skip from log

static const char *TAG = "sensorTask";

const char measLabelTxt[][20] = {{"Luchtkwaliteitindex"}, {"CO2"}, {"TVOC"}, {"Rs0"}, {"Rs1"}, {"Rs2"}, {"Rs3"}};

// extern i2c_master_dev_handle_t SPS30_dev_handle;
extern int scriptState;
extern uint32_t timeStamp;

uint16_t ECO2;
uint16_t Tvoc;
uint32_t Rs0;
uint32_t Rs1;
uint32_t Rs2;
uint32_t Rs3;
uint8_t AQi;
typedef struct {
	int values[7];
} lastVal_t;

log_t logLastVal;
lastVal_t lastVal;

// I2C (Wire): GPIO 8 (SDA) and GPIO 9 (SCL). You can reassign pins in code before Wire.begin() (e.g., Wire.begin(sdaPin, sclPin)).

// esp_err_t SPS30AddDeviceToBus(i2c_master_bus_handle_t *bus_handle) {

// 	i2c_device_config_t dev_config = {
// 		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
// 		.device_address = SPS30_ADDR,
// 		.scl_speed_hz = CLKSPEED,
// 	};
// 	return i2c_master_bus_add_device(*bus_handle, &dev_config, &SPS30_dev_handle);
// }

Averager averager1[NR_MEASVALUES];
Averager averager2[NR_MEASVALUES];

Log hourLog(3600 / HOURLOGINTERVAL, sizeof(log_t));	   // 10 sec interval
Log dayLog((24 * 60) / DAYLOGINTERVAL, sizeof(log_t)); // 5 min interval

#include <Arduino.h>
#include <ScioSense_ENS16x.h>
#include <Wire.h>

#define I2C_ADDRESS 0x53

// #define USE_INTERRUPT		// enable in case you're using a dedicated interrupt pin
#define INTN 2

// S3: SDA 8 , SCL 9

ENS160 ens16x;

void setup() {

	ens16x.enableDebugging(Serial);

	Wire.begin();
	ens16x.begin(&Wire, I2C_ADDRESS);

	printf("begin..\n");
	while (ens16x.init() != true) {
		printf(".\n");
		delay(1000);
	}
	printf("success\n");

#ifdef USE_INTERRUPT
	ens16x.setInterruptPin(INTN);
	ens16x.writeConfiguration(ENS16X_CONFIGURATION_INTERRUPT_ENABLE | ENS16X_CONFIGURATION_NEW_GENERAL_PURPOSE_DATA | ENS16X_CONFIGURATION_NEW_DATA);
#endif

	ens16x.startStandardMeasure();
}

// void loop() {
// 	ens16x.wait();
// 	if (ens16x.update() == RESULT_OK) {
// 		if (ens16x.hasNewData()) {
// 			printf("\nAQI UBA:");
// 			printf("%d", (uint8_t)ens16x.getAirQualityIndex_UBA());
// 			printf("\tTVOC:");
// 			printf("%d", ens16x.getTvoc());
// 			printf("\tECO2:");
// 			printf("%d", ens16x.getEco2());

// 			AQi = ens16x.getAirQualityIndex_UBA();
// 			Tvoc = ens16x.getTvoc();
// 			ECO2 = ens16x.getEco2();
// 		}

// 		if (ens16x.hasNewGeneralPurposeData()) {
// 			printf("\nRS0:");
// 			printf("%ld", ens16x.getRs0());
// 			printf("\tRS1:");
// 			printf("%ld", ens16x.getRs1());
// 			printf("\tRS2:");
// 			printf("%ld", ens16x.getRs2());
// 			printf("\tRS3:");
// 			printf("%ld", ens16x.getRs3());

// 			Rs0 = ens16x.getRs0();
// 			Rs1 = ens16x.getRs1();
// 			Rs2 = ens16x.getRs2();
// 			Rs3 = ens16x.getRs3();
// 		}
// 	}
// }

void sensorTask(void *parameters) {
	esp_err_t error = ESP_OK;
	uint32_t device_status = 0;
	displayMssg_t displayMssg;
	char str[20];
	log_t logValue;
	int lastMinute = -1;
	int lastSecond = -1;
	time_t now = 0;
	struct tm timeinfo;
	int logDayPrescaler = 1;
	int logHourPrescaler = 1;
	int values[NR_MEASVALUES];
	float avGvalues[NR_MEASVALUES];
	bool statusError;
	int skipFirstSamplesFromLog = SKIPFIRSTSAMPLES;
	int timeOutTimer = TIMEOUT;

	setup();

	for (int n = 0; n < NR_MEASVALUES; n++) {
		averager1[n].setAverages(AVERAGES1);
		averager2[n].setAverages(AVERAGES2);
	}

	// do {
	// 	error = i2c_master_probe(bus_handle, SPS30_ADDR, 50);
	// 	if (error) {
	// 		ESP_LOGE(TAG, "SPS30 sensor not detected", error);
	// 		vTaskDelay(1000 / portTICK_PERIOD_MS);
	// 	}
	// } while (error);

	// error = SPS30AddDeviceToBus(&bus_handle);
	// if (error)
	// 	ESP_LOGE(TAG, "Failed to add SPS30 to bus", error);

	// sps30_stop_measurement();
	// int8_t serial_number[32] = {0};
	// int8_t product_type[8] = {0};
	// do {
	// 	error = sps30_read_serial_number(serial_number, 32);
	// 	if (error != ESP_OK) {
	// 		ESP_LOGE(TAG, "Failed to read serialnr %d", error);
	// 		vTaskDelay(1000 / portTICK_PERIOD_MS);
	// 	}
	// } while (error != ESP_OK);

	// ESP_LOGI(TAG, "SPS 30 serial_number: %s\n", serial_number);
	// sps30_read_product_type(product_type, 8);
	// ESP_LOGI(TAG, "SPS 30 product_type: %s\n", product_type);

	// //	sps30_start_measurement((sps30_output_format)(1280));
	// sps30_start_measurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
	// vTaskDelay(1 / portTICK_PERIOD_MS);

	uint16_t data_ready_flag = 0;
	displayMssg.str1 = str;
	// uint16_t mc_1p0 = 0;
	// uint16_t mc_2p5 = 0;
	// uint16_t mc_4p0 = 0;
	// uint16_t mc_10p0 = 0;
	// uint16_t nc_0p5 = 0;
	// uint16_t nc_1p0 = 0;
	// uint16_t nc_2p5 = 0;
	// uint16_t nc_4p0 = 0;
	// uint16_t nc_10p0 = 0;
	// uint16_t typical_particle_size = 0;
	//   uint16_t repetition = 0;
	//   for (repetition = 0; repetition < 50; repetition++) {
	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
		if (timeOutTimer)
			timeOutTimer--;
		else {
			printf("ENS160 timeout\n\r");
			ens16x.reset();
			ens16x.startStandardMeasure();
			timeOutTimer = TIMEOUT;
		}
		statusError = false;
		if (ens16x.update() == RESULT_OK) {
			if (ens16x.hasNewData()) {
				timeOutTimer = TIMEOUT;

				AQi = ens16x.getAirQualityIndex_UBA();
				Tvoc = ens16x.getTvoc();
				ECO2 = ens16x.getEco2();

				printf("\nAQI UBA:");
				printf("%d", (uint8_t)AQi);
				printf("\tTVOC:");
				printf("%d", Tvoc);
				printf("\tECO2:");
				printf("%d",ECO2);
		
			}

			if (ens16x.hasNewGeneralPurposeData()) {
				Rs0 = ens16x.getRs0();
				Rs1 = ens16x.getRs1();
				Rs2 = ens16x.getRs2();
				Rs3 = ens16x.getRs3();
				
				printf("\nRS0:");
				printf("%ld", Rs0);
				printf("\tRS1:");
				printf("%ld", Rs1);
				printf("\tRS2:");
				printf("%ld", Rs2);
				printf("\tRS3:");
				printf("%ld", Rs3);
				
				data_ready_flag = true;
			}
		}
		if (statusError) {
			displayMssg.displayItem = DISPLAY_ITEM_STATUSLINE;
			if (displayMssgBox != NULL)
				xQueueSend(displayMssgBox, &displayMssg, 0);
		} else {

			displayMssg.displayItem = DISPLAY_ITEM_MEASLINE;

			if (data_ready_flag) {
				data_ready_flag = false;
				values[0] = AQi;
				values[1] = ECO2;
				values[2] = Tvoc;
				values[3] = Rs0;
				values[4] = Rs1;
				values[5] = Rs2;
				values[6] = Rs3;
				if (skipFirstSamplesFromLog)
					skipFirstSamplesFromLog--;
				else {
					for (int n = 0; n < NR_MEASVALUES; n++) { // average mc's
						switch (n) {
						case 0:
						case 1:
						case 2:
						case 4:
							averager1[n].write((int)1000.0 * values[n]);
							break;
						default:
							averager1[n].write(values[n]); // value R0 ,2,3   > 100000
							break;
						}
						avGvalues[n] = averager1[n].average() / 1000;
						lastVal.values[n] = (int)avGvalues[n]; // for cgi sensorInfo
						logLastVal.values[n] = avGvalues[n];   // for cgi pringtlog;
					}
					logLastVal.timeStamp = timeStamp;
					displayMssg.values = &avGvalues[0];
					if (displayMssgBox != NULL)
						xQueueSend(displayMssgBox, &displayMssg, 0);

					time(&now);
					localtime_r(&now, &timeinfo);
					if (lastMinute == -1) {
						lastMinute = timeinfo.tm_min;
						lastSecond = timeinfo.tm_sec;
					}
					if (lastSecond != timeinfo.tm_sec) {
						lastSecond = timeinfo.tm_sec; // every second

						if (logHourPrescaler > 0)
							logHourPrescaler--;
						else {
							logHourPrescaler = HOURLOGINTERVAL - 1;

							for (int n = 0; n < NR_MEASVALUES; n++) {
								avGvalues[n] = averager1[n].average();
								averager2[n].write(avGvalues[n]); // second order for daylog
							}

							for (int n = 0; n < NR_MEASVALUES; n++) {		  // average mc's
								avGvalues[n] = averager1[n].average() / 1000; // contains last samples
							}
							memcpy(logValue.values, avGvalues, sizeof(logValue.values));
							logValue.timeStamp = timeStamp;
							hourLog.add(&logValue);
						}

						if (lastMinute != timeinfo.tm_min) {
							lastMinute = timeinfo.tm_min; // every minute

							if (logDayPrescaler > 0) {
								logDayPrescaler--;
							} else {
								logDayPrescaler = DAYLOGINTERVAL - 1;	  // reset prescaler
								for (int n = 0; n < NR_MEASVALUES; n++) { // average mc's
									avGvalues[n] = averager2[n].average() / 1000;
								}
								memcpy(logValue.values, avGvalues, sizeof(logValue.values));
								logValue.timeStamp = timeStamp;
								dayLog.add(&logValue);
							}
						}
					}
				}
			}
		}
	}
}

// CGI stuff

int printLog(log_t *logToPrint, char *pBuffer) {
	int len = 0;
	len = sprintf(pBuffer, "%ld,", logToPrint->timeStamp);
	for (int n = 0; n < NR_MEASVALUES; n++)
		len += sprintf(pBuffer + len, "%3.0f,", logToPrint->values[n]);
	len += sprintf(pBuffer + len, "\n");
	return len;
}

// reads all available data from log
// issued as first request.

int getDayLogsScript(char *pBuffer, int count) {
	int left, len = 0;
	static int logsToSend = -1;
	if (logsToSend < 0)
		logsToSend = dayLog.getNrLogsToSend();

	if (logsToSend) {
		len = 0;
		do {
			len += printLog((log_t *)dayLog.readNext(), pBuffer + len);
			logsToSend--;
			left = count - len;
		} while (logsToSend && (left > (len + 50)));
	} else
		logsToSend = -1;
	return len;
}

int getHourLogsScript(char *pBuffer, int count) {
	int left, len = 0;
	static int logsToSend = -1;
	if (logsToSend < 0)
		logsToSend = hourLog.getNrLogsToSend();

	if (logsToSend) {
		len = 0;
		do {
			len += printLog((log_t *)hourLog.readNext(), pBuffer + len);
			logsToSend--;
			left = count - len;
		} while (logsToSend && (left > (len + 50)));
	} else
		logsToSend = -1;
	return len;
}

int clearLogScript(char *pBuffer, int count) {
	dayLog.clear();
	hourLog.clear();
	if (scriptState == 0) {
		strcpy(pBuffer, "OK");
		scriptState++;
		return 3;
	}
	return 0;
}

int getRTMeasValuesScript(char *pBuffer, int count) {
	int len = 0;
	switch (scriptState) {
	case 0:
		scriptState++;
		len += printLog(&logLastVal, pBuffer);
		return len;
		break;
	default:
		break;
	}
	return 0;
}

const CGIdesc_t sensorInfoDescriptorTable[] = {{measLabelTxt[0], &lastVal.values[0], INT, 1}, {measLabelTxt[1], &lastVal.values[1], INT, 1},
											   {measLabelTxt[2], &lastVal.values[2], INT, 1}, {measLabelTxt[3], &lastVal.values[3], INT, 1},
											   {measLabelTxt[4], &lastVal.values[4], INT, 1}, {measLabelTxt[5], &lastVal.values[5], INT, 1},
											   {measLabelTxt[6], &lastVal.values[6], INT, 1}, {NULL, NULL, INT, 0}};

// const CGIdesc_t sensorInfoDescriptorTable[] = {{measLabelTxt[5], &lastVal.values[5], FLT, 1}, {NULL, NULL, INT, 0}};

int getSensorInfoScript(char *pBuffer, int count) {
	int len = 0;

	switch (scriptState) {
	case 0:
		scriptState++;
		len = sprintf(pBuffer, "Meting, Waarde\n");
		len += getCGItable(sensorInfoDescriptorTable, pBuffer + len, count);
		return len;
		break;
	default:
		break;
	}
	return 0;
}
