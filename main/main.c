/*
	VCP - WebSocket bridge Example

	This example code is in the Public Domain (or CC0 licensed, at your option.)
	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "cJSON.h"
#include "esp_sntp.h"

#include "websocket_server.h"

static const char *TAG = "MAIN";

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define sntp_setoperatingmode esp_sntp_setoperatingmode
#define sntp_setservername esp_sntp_setservername
#define sntp_init esp_sntp_init
#endif

MessageBufferHandle_t xMessageBufferRx;
MessageBufferHandle_t xMessageBufferTx;

// The total number of bytes (not messages) the message buffer will be able to hold at any one time.
size_t xBufferSizeBytes = 1024;
// The size, in bytes, required to hold each item in the message,
size_t xItemSize = 256;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

esp_err_t wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&event_handler,
		NULL,
		&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&event_handler,
		NULL,
		&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	esp_err_t ret_value = ESP_OK;
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
		ret_value = ESP_FAIL;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		ret_value = ESP_FAIL;
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
	return ret_value;
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

	//initialize service
	ESP_ERROR_CHECK( mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0) );
#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}

void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
	ESP_LOGI(TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	//sntp_setservername(0, "pool.ntp.org");
	ESP_LOGI(TAG, "Your NTP Server is %s", CONFIG_NTP_SERVER);
	sntp_setservername(0, CONFIG_NTP_SERVER);
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	sntp_init();
}

static esp_err_t obtain_time(void)
{
	initialize_sntp();
	// wait for time to be set
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	if (retry == retry_count) return ESP_FAIL;
	return ESP_OK;
}

#if 0
#define RX_BUF_SIZE		128

void uart_init(void) {
	const uart_config_t uart_config = {
		//.baud_rate = 115200,
		.baud_rate = CONFIG_UART_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
		.source_clk = UART_SCLK_DEFAULT,
#else
		.source_clk = UART_SCLK_APB,
#endif
	};
	// We won't use a buffer for sending data.
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	//uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_set_pin(UART_NUM_1, CONFIG_UART_TX_GPIO, CONFIG_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void uart_tx(void* pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Start using GPIO%d", CONFIG_UART_TX_GPIO);
	char* messageBuffer = malloc(xItemSize+1);
	if (messageBuffer == NULL) {
		ESP_LOGE(pcTaskGetName(NULL), "messageBuffer malloc Fail");
		while(1) { vTaskDelay(1); }
	}
	char* txBuffer = malloc(xItemSize+1);
	if (txBuffer == NULL) {
		ESP_LOGE(pcTaskGetName(NULL), "txBuffer malloc Fail");
		while(1) { vTaskDelay(1); }
	}

	//char messageBuffer[xItemSize];
	while(1) {
		size_t received = xMessageBufferReceive(xMessageBufferTx, messageBuffer, xItemSize, portMAX_DELAY );
		ESP_LOGI(pcTaskGetName(NULL), "received=%d", received);
		cJSON *root = cJSON_Parse(messageBuffer);
		if (cJSON_GetObjectItem(root, "id")) {
			char *id = cJSON_GetObjectItem(root,"id")->valuestring;
			char *payload = cJSON_GetObjectItem(root,"payload")->valuestring;
			ESP_LOGD(pcTaskGetName(NULL), "id=%s",id);
			int payloadLen = strlen(payload);
			ESP_LOGI(pcTaskGetName(NULL), "payloadLen=%d payload=[%s]",payloadLen, payload);
			if (payloadLen) {
				ESP_LOG_BUFFER_HEXDUMP(pcTaskGetName(NULL), payload, payloadLen, ESP_LOG_INFO);
				strncpy(txBuffer, payload, payloadLen);
				txBuffer[payloadLen] = 0x0a;
				ESP_LOG_BUFFER_HEXDUMP(pcTaskGetName(NULL), txBuffer, payloadLen+1, ESP_LOG_INFO);
				//int txBytes = uart_write_bytes(UART_NUM_1, payload, payloadLen);
				int txBytes = uart_write_bytes(UART_NUM_1, txBuffer, payloadLen+1);
				ESP_LOGD(pcTaskGetName(NULL), "txBytes=%d", txBytes);
			}
		}
		cJSON_Delete(root);
	} // end while

	// Never reach here
	free(messageBuffer);
	free(txBuffer);
	vTaskDelete(NULL);
}

static void uart_rx(void* pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Start using GPIO%d", CONFIG_UART_RX_GPIO);
	uint8_t* rxBuffer = (uint8_t*) malloc(xItemSize+1);
	if (rxBuffer == NULL) {
		ESP_LOGE(pcTaskGetName(NULL), "rxBuffer malloc Fail");
		while(1) { vTaskDelay(1); }
	}
	char* payload = malloc(xItemSize+1);
	if (payload == NULL) {
		ESP_LOGE(pcTaskGetName(NULL), "payload malloc Fail");
		while(1) { vTaskDelay(1); }
	}

	while (1) {
		int received = uart_read_bytes(UART_NUM_1, rxBuffer, xItemSize, 10 / portTICK_PERIOD_MS);
		// There is some buffer in rx buffer
		if (received > 0) {
			rxBuffer[received] = 0;
			ESP_LOGI(pcTaskGetName(NULL), "received=%d", received);
			ESP_LOG_BUFFER_HEXDUMP(pcTaskGetName(NULL), rxBuffer, received, ESP_LOG_INFO);

			int index = 0;
			int ofs = 0;
			while(1) {
				if (rxBuffer[index] == 0x0d) {

				} else if (rxBuffer[index] == 0x0a) {
					cJSON *root;
					root = cJSON_CreateObject();
					cJSON_AddStringToObject(root, "id", "recv-request");
					cJSON_AddStringToObject(root, "payload", payload);
					//char *my_json_string = cJSON_Print(root);
					char *my_json_string = cJSON_PrintUnformatted(root);
					ESP_LOGD(pcTaskGetName(NULL), "my_json_string=[%s]",my_json_string);
					cJSON_Delete(root);
					xMessageBufferSend(xMessageBufferRx, my_json_string, strlen(my_json_string), portMAX_DELAY);
					cJSON_free(my_json_string);

					ofs = 0;
				} else {
					payload[ofs++] = rxBuffer[index];
					payload[ofs] = 0;
				}
				index++;
				if (index == received) break;
			} // end while

		} else {
			// There is no data in rx buufer
			//ESP_LOGI(pcTaskGetName(NULL), "Read %d", received);
		}
	} // end while

	// Never reach here
	free(rxBuffer);
	free(payload);
	vTaskDelete(NULL);
}
#endif

void client_task(void* pvParameters);
void server_task(void* pvParameters);
void cdc_acm_vcp_task(void *pvParameters);

void app_main() {
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Initialize WiFi
	ESP_ERROR_CHECK(wifi_init_sta());

#if 0
	// Initialize uart
	uart_init();
#endif

	// Initialize mdns
	initialise_mdns();

	// Obtain time over NTP
	ESP_ERROR_CHECK(obtain_time());

	// Create MessageBuffer
	xMessageBufferRx = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferRx );
	xMessageBufferTx = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferTx );

	// Get the local IP address
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
	char cparam0[64];
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));
	//sprintf(cparam0, "%s.local", CONFIG_MDNS_HOSTNAME);

	// Start web socket server
	ws_server_start();

	// Start web server
	xTaskCreate(&server_task, "server_task", 1024*4, (void *)cparam0, 5, NULL);

	// Start web client
	xTaskCreate(&client_task, "client_task", 1024*4, NULL, 5, NULL);

    // Start CDC_ACM_VCP
    xTaskCreate(cdc_acm_vcp_task, "CDC_ACM_VCP", 1024*4, NULL, 2, NULL);

	vTaskDelay(100);
}
