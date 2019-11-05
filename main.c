/* 	
*	
*	Wlacznik swiatla, program napisany na modul ESP8266-01. 
*	Posiada jedno wyjscie i jedno wejscie, do 
*	ktorego podlaczony jest przycisk.
* 	Za pomoca wspomnianego przycisku mozna zmieniac 
*	stan wyjscia oraz po przucisnieciu go na czas 10s 
*	zresetowac ustawienia WiFi. Kamil Miedzinski
*
*
*/

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "button.h"


//************************ Deklaracja funkcji *******************************

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void button_callback(uint8_t gpio, button_event_t event);

void reset_configuration_task();
void reset_configuration();

//************************ Deklaracja zmiennych *******************************


const int relay_gpio = 2;  // GPIO do podlaczenia przekaznika.

const int button_gpio = 0; // GPIO do podlaczenia przycisku Przelaczania/Resetowania WiF

// Zmiennna Homekit do sterowania wyjsciem 
homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(
    ON, true, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
);

// Zmiennna Homekit przechowujaca domyslna nazwe urzadzeni 
homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "LightBulb");


// ******** Funkcja zmieniajaca stan na wyjsciu *******
void relay_write(bool on) {
    gpio_write(relay_gpio, on ? 1 : 0);
}

//******* Funkcja inicjalizacji GPIO jako wyjscia ********
void gpio_init() {
    gpio_enable(relay_gpio, GPIO_OUTPUT);
    relay_write(switch_on.value.bool_value);
}

//******* Funkcja zwracajaca stan wyjscia do aplikacji Home ********
void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch_on.value.bool_value);
}

// ******* Funkcja resetowania konfiguracji WiFi *******
void reset_configuration_task() {

    printf("Resetting Wifi Config\n");
    
    wifi_config_reset(); // Reset konfiguracji Wifi
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    printf("Resetting HomeKit Config\n");
    
    homekit_server_reset(); // Reset konfiguracji Homekit
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    printf("Restarting\n");
    
    sdk_system_restart(); // Reset modulu ESP8266
    
    vTaskDelete(NULL);
}

// ****** Zadanie uzywajace funkcji resetowania konfiguracji Wifi ******
void reset_configuration() {
    printf("Resetting configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

//******* Funkcja wykrywajaca wcisniety przycisk ******** 
void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("Toggling relay\n");
            switch_on.value.bool_value = !switch_on.value.bool_value;
            relay_write(switch_on.value.bool_value);
            homekit_characteristic_notify(&switch_on, switch_on.value);
            break;
        case button_event_long_press:
            reset_configuration();
            break;
        default:
            printf("Unknown button event: %d\n", event);
    }
}
// ****** Zadanie uzywajace funkcji identyfikacji ******
void switch_identify_task(void *_args) {
    vTaskDelete(NULL);
}
// ****** Funkcjia identyfikacji podlaczonego do aplikacji Home urzadzenia ******
void switch_identify(homekit_value_t _value) {
    printf("LightBulb identify\n");
    xTaskCreate(switch_identify_task, "LightBulb identify", 128, NULL, 2, NULL);
}


//******* Parametry urzadzenia  Homekit przechowujaca domyslna nazwe urzadzenia ********
homekit_accessory_t *accessories[] = {	
	// Akcesorium Homekit
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]){
	// Serwis Homekit - nazwa urzadzenia
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Homekit"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "22052019BF1D"), 
            HOMEKIT_CHARACTERISTIC(MODEL, "LightBulb"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            NULL
        }),
	// Serwis Homekit - zarowka
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "LightBulb"),
            &switch_on,
            NULL
        }),
        NULL
    }),
    NULL
};
//******* Zmiennna Homekit przechowujaca zmienne konfiguracyjne ********
homekit_server_config_t config = {
    .accessories = accessories,
    .password = "123-45-678",
    .setupId="1QJ8",
};
// ****** Funkcjia inicjalizujaca serwer Homekit ******
void on_wifi_ready() {
    homekit_server_init(&config);
}
// ****** Funkcjia nadajaca nazwe sieci WiFi ******
void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    int name_len = snprintf(NULL, 0, "LightBulb-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "LightBulb-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
}
// ****** Funkcjia inicjalizacji modulu ******
void user_init(void) {
    uart_set_baud(0, 115200);

    create_accessory_name();
    
    wifi_config_init("LightBulb", NULL, on_wifi_ready);
    gpio_init();

    if (button_create(button_gpio, 0, 4000, button_callback)) {
        printf("Failed to initialize button\n");
    }
}
