#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <time.h>
#include "esp_sntp.h"

#include "ssd1306.h"

#define tag "SSD1306"

#define WIFI_SSID "AndroidAPFA59"
#define WIFI_PASSWORD "Xrda6605"

#define BUTTON_GPIO_NEXT 12
#define BUTTON_GPIO_WHEEL 13

#define DISPLAY_UPDATE_INTERVAL_MS 1000

SSD1306_t dev;

int currentPage = 0;
float wheelCircumference = 2.145f; // in meters
int32_t roundCount = 0; 

float currentSpeed = 0.0f;
float avgSpeed = 0.0f;
float distance = 0.0f;

int64_t currentTime = 0;
int64_t now = 0;
int64_t lastTime = 0;
int64_t round1 = 0;
int64_t round2 = 0;
int64_t pageButtonPressed = 0;

bool reset = false;
bool standBy = true;
bool switchOff = false;

void send_request(void);

void calculate_speed_distance() {
    distance = roundCount * wheelCircumference; // in meters

    int32_t timeInBetween = round1 - round2; // in ms

    if((int64_t)(now - round1) >= 15000 && (int64_t)(now - pageButtonPressed) >= 12000){ //12s
        send_request();
        vTaskDelay(200 / portTICK_PERIOD_MS);
        switchOff = true;
    }else if((now - round1) > 4000){ //4s
        currentSpeed = 0;
        standBy = true;
    }else if(timeInBetween == 0){
        currentSpeed = currentSpeed;
    }else if(currentTime != 0){
        currentSpeed = wheelCircumference / (timeInBetween / 1000.0); // in m/s
        avgSpeed = distance / (currentTime / 1000.0); // in m/s
    }
}

void button_task(void *pvParameter) {
    int64_t buttonHold = 0;
    bool rst = false;
    bool buttonPressed = false;
    bool didReset = false;
    while(1) {
        if(gpio_get_level(BUTTON_GPIO_NEXT) == 0){
            if(!rst){
                currentPage = (currentPage + 1) % 3;
                switchOff = false;
                rst = true;
                pageButtonPressed = xTaskGetTickCount() * portTICK_PERIOD_MS;
                vTaskDelay(300 / portTICK_PERIOD_MS);
            }else{
                buttonHold = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if(buttonHold - pageButtonPressed >= 2000){
                    if(!didReset){
                        didReset = true;
                        reset = true;
                        printf("data reset\n");
                    }
                }
            }
        }else{
            rst = false;
            didReset = false;
        }

        if(gpio_get_level(BUTTON_GPIO_WHEEL) == 0) {
            if(!buttonPressed){ 
                buttonPressed = true;
                roundCount++;
                round2 = round1;
                round1 = xTaskGetTickCount() * portTICK_PERIOD_MS;
                switchOff = false;
                standBy = false;
            }
        }else{
            buttonPressed = false;
        }
        vTaskDelay(42 / portTICK_PERIOD_MS);
    }
}

void display_page() {
    char display_value[16];
    switch (currentPage) {
        case 0:
            ssd1306_contrast(&dev, 0xff);
            ssd1306_display_text(&dev, 0, "Current speed:", 14, false);
            sprintf(display_value, " %.3f m/s", currentSpeed);
            break;
        case 1:
            ssd1306_contrast(&dev, 0xff);
            ssd1306_display_text(&dev, 0, "Avg. speed:   ", 14, false);
            sprintf(display_value, " %.3f m/s", avgSpeed);
            break;
        case 2:
            ssd1306_contrast(&dev, 0xff);
            ssd1306_display_text(&dev, 0, "Distance:     ", 14, false);
            sprintf(display_value, " %.3f m", distance);
            break;
    }
    for(int i = strlen(display_value); i < 16; i++) {
        display_value[i] = ' ';
    }
    ssd1306_display_text(&dev, 2, display_value, 16, false);
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        }   
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start();

    while(esp_wifi_connect() != ESP_OK) {
        printf("WiFi connecting... \n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void init_sntp(){
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

char* getTimestamp() {
    while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    char *timestamp = malloc(64);
    if(timestamp == NULL) {
        ESP_LOGE(tag, "Failed to allocate memory for timestamp");
        return NULL;
    }
    time_t now;
    struct tm timeinfo;
    time(&now);
    setenv("TZ", "UTC-1", 1);
    localtime_r(&now, &timeinfo);
    strftime(timestamp, 64, "%d.%m.%Y %H:%M:%S", &timeinfo);
    return timestamp;
}

void send_request() {
    esp_http_client_config_t config = {
        .url = "http://www.stud.fit.vutbr.cz/~xstrei06/IMP/index.php",
        .method = HTTP_METHOD_POST,
        .event_handler = NULL,
        .cert_pem = NULL,
        .buffer_size = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(tag, "Failed to initialize HTTP client");
        return;
    }

    init_sntp();
    char data[128] = {0};
    char *timestamp = getTimestamp();
    sprintf(data, "%s, average speed: %.3f m/s, distance: %.3f m\n", timestamp, avgSpeed, distance);
    free(timestamp);
    esp_sntp_stop();
    
    esp_err_t post_field = esp_http_client_set_post_field(client, data, strlen(data));
    if(post_field != ESP_OK){
        ESP_LOGE(tag, "Failed to set post field: %s", esp_err_to_name(post_field));
    }

    esp_err_t header = esp_http_client_set_header(client, "Content-Type", "text/plain");
    if(header != ESP_OK){
        ESP_LOGE(tag, "Failed to set header: %s", esp_err_to_name(header));
    }
    
    esp_err_t perform = esp_http_client_perform(client);
    if (perform != ESP_OK) {
        ESP_LOGE(tag, "HTTP Request failed: %s", esp_err_to_name(perform));
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGE(tag, "Status = %d", status_code);
    } else {
        ESP_LOGI(tag, "HTTP Request succeeded");
    }
    esp_http_client_cleanup(client);
}

void nvs_save(nvs_handle_t handle){
    char save_float[50] = {0};
    sprintf(save_float, "%f", avgSpeed);
    nvs_set_str(handle, "avgSpeed", save_float);
    nvs_set_i32(handle, "roundCount", roundCount);
    nvs_set_i64(handle, "currentTime", currentTime);
}

void nvs_load(nvs_handle_t handle){
    char load_float[50] = {0};
    size_t length = sizeof(load_float);
    nvs_get_str(handle, "avgSpeed", load_float, &length);
    avgSpeed = atof(load_float);
    nvs_get_i32(handle, "roundCount", &roundCount);
    nvs_get_i64(handle, "currentTime", &currentTime);
}

void app_main(void)
{
    spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
	ssd1306_init(&dev, 128, 64);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text_x3(&dev, 0, "TACHO", 5, false);
    ssd1306_display_text_x3(&dev, 3, "METER", 5, false);

    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);

    nvs_handle_t nvs_handle;
    ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    nvs_load(nvs_handle);

    wifi_connection();

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // init buttons
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO_NEXT) | (1ULL << BUTTON_GPIO_WHEEL),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&io_conf);

    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ssd1306_clear_screen(&dev, false);

    while(1) {
        if(reset){
            reset = false;
            roundCount = 0;
            currentTime = 0;
            avgSpeed = 0;
            distance = 0;
            display_page();
            send_request();
            vTaskDelay(300 / portTICK_PERIOD_MS);
            nvs_save(nvs_handle);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        if(switchOff){
            ssd1306_clear_screen(&dev, false);
            ssd1306_contrast(&dev, 0xff);
            while(switchOff){
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }

        now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if(!standBy){
            currentTime += now - lastTime;
        }

        lastTime = now;

        calculate_speed_distance();

        display_page();

        nvs_save(nvs_handle);

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
