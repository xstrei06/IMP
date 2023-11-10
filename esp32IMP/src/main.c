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

int currentPage = 0;
float wheelCircumference = 2.145f; // in meters
int roundCount = 0; 

float currentSpeed = 0.0f;
float avgSpeed = 0.0f;
float distance = 0.0f;
uint32_t time = 0;
uint32_t round1 = 0;
uint32_t round2 = 0;

void calculate_speed_distance() {
    distance = roundCount * wheelCircumference; // in meters

    if(time == 0){
        avgSpeed = 0;
    }else{
        avgSpeed = distance / (time / 1000.0); // in m/s
    }
    
    uint32_t timeInBetween = round1 - round2; // in ms
    if((time - round1) > 2000){
        currentSpeed = 0;
    }else if(timeInBetween == 0){
        currentSpeed = currentSpeed;
    }else{
        currentSpeed = wheelCircumference / (timeInBetween / 1000.0); // in m/s
    }
    
    printf("currentSpeed: %f\n", currentSpeed);
    printf("avgSpeed: %f\n", avgSpeed);
}

void button_task(void *pvParameter) {
    while(1) {
        if(gpio_get_level(BUTTON_GPIO_NEXT) == 0) {
            currentPage = (currentPage + 1) % 3;
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }

        if(gpio_get_level(BUTTON_GPIO_WHEEL) == 0) {
            roundCount++;
            round2 = round1;
            round1 = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGI(tag, "Wheel button pressed. roundCount: %d", roundCount);
            calculate_speed_distance();
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void display_page(SSD1306_t *dev, int page, float currentSpeed, float avgSpeed, float distance) {
    switch (page) {
        case 0:
            ssd1306_clear_screen(dev, false);
            ssd1306_contrast(dev, 0xff);
            ssd1306_display_text(dev, 0, "Current speed:", 14, false);
            char speed_text[30];
            sprintf(speed_text, " %f m/s", currentSpeed);
            ssd1306_display_text(dev, 2, speed_text, strlen(speed_text), false);
            break;
        case 1:
            ssd1306_clear_screen(dev, false);
            ssd1306_contrast(dev, 0xff);
            ssd1306_display_text(dev, 0, "Avg. speed:", 11, false);
            char avg_speed_text[30];
            sprintf(avg_speed_text, " %f m/s", avgSpeed);
            ssd1306_display_text(dev, 2, avg_speed_text, strlen(avg_speed_text), false);
            break;
        case 2:
            ssd1306_clear_screen(dev, false);
            ssd1306_contrast(dev, 0xff);
            ssd1306_display_text(dev, 0, "Distance:", 9, false);
            char distance_text[30];
            sprintf(distance_text, " %f m", distance);
            ssd1306_display_text(dev, 2, distance_text, strlen(distance_text), false);
            break;
    }
}

void app_main(void)
{

	SSD1306_t dev;

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

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    while(1) {

        time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        calculate_speed_distance();

        display_page(&dev, currentPage, currentSpeed, avgSpeed, distance);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
