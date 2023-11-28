#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ssd1306.h"

#define tag "SSD1306"

#define BUTTON_GPIO_NEXT 12
#define BUTTON_GPIO_WHEEL 13

#define DISPLAY_UPDATE_INTERVAL_MS 1000

SSD1306_t dev;

int currentPage = 0;
float wheelCircumference = 2.145f; // in meters
int roundCount = 0; 

float currentSpeed = 0.0f;
float avgSpeed = 0.0f;
float distance = 0.0f;

uint32_t time = 0;
int64_t now = 0;
uint32_t lastTime = 0;
uint32_t round1 = 0;
uint32_t round2 = 0;
uint32_t pageButtonPressed = 0;

bool reset = false;
bool standBy = true;
bool switchOff = false;

void calculate_speed_distance() {
    distance = roundCount * wheelCircumference; // in meters

    if(time == 0){
        avgSpeed = 0;
    }    

    uint32_t timeInBetween = round1 - round2; // in ms
    if((int64_t)(now - round1) >= 15000 && (int64_t)(now - pageButtonPressed) >= 15000){ //15s
        switchOff = true;
    }else if((now - round1) > 4000){ //4s
        currentSpeed = 0;
        standBy = true;
    }else if(timeInBetween == 0){
        currentSpeed = currentSpeed;
    }else{
        currentSpeed = wheelCircumference / (timeInBetween / 1000.0); // in m/s
        avgSpeed = distance / (time / 1000.0); // in m/s
    }
}

void button_task(void *pvParameter) {
    uint32_t buttonHold = 0;
    bool rst = false;
    bool buttonPressed = false;
    while(1) {
        if(gpio_get_level(BUTTON_GPIO_NEXT) == 0) {
            if(!rst){
                currentPage = (currentPage + 1) % 3;
                switchOff = false;
                rst = true;
                standBy = false;
                pageButtonPressed = xTaskGetTickCount() * portTICK_PERIOD_MS;
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }else{
                buttonHold = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if(buttonHold - pageButtonPressed >= 3000){
                    reset = true;
                    standBy = true;
                }
            }
        }else{
            rst = false;
        }

        if(gpio_get_level(BUTTON_GPIO_WHEEL) == 0) {
            if(!buttonPressed){ 
                buttonPressed = true;
                roundCount++;
                round2 = round1;
                round1 = xTaskGetTickCount() * portTICK_PERIOD_MS;
                standBy = false;
                switchOff = false;
            }
        }else{
            buttonPressed = false;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
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

void app_main(void)
{

	ESP_LOGI(tag, "INTERFACE is SPI");
	ESP_LOGI(tag, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(tag, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(tag, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);
	ESP_LOGI(tag, "CONFIG_DC_GPIO=%d",CONFIG_DC_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);

    // Initialize buttons
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL<<BUTTON_GPIO_NEXT) | (1ULL<<BUTTON_GPIO_WHEEL);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text_x3(&dev, 0, "Hello", 5, false);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    ssd1306_clear_screen(&dev, false);

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    while(1) {
        if(reset){
            roundCount = 0;
            time = 0;
            round1 = 0;
            round2 = 0;
            distance = 0;
            avgSpeed = 0;
            currentSpeed = 0;
            reset = false;
        }

        now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if(!standBy){
            time += now - lastTime;
        }

        lastTime = now;

        calculate_speed_distance();

        display_page();

        if(switchOff){
            ssd1306_clear_screen(&dev, false);
            ssd1306_contrast(&dev, 0xff);
            while(switchOff){
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
