#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "ws2812b.pio.h"

#define LED_COUNT 25
#define LED_PIN 7
#define LED_RED 13
const uint BTN_A = 5;
const uint BTN_B = 6;

static volatile uint8_t contador = 0;
static void gpio_irq_handler(uint gpio, uint32_t events);
static volatile uint32_t tmp_ant = 0;

// Estrutura para armazenar cor do LED
typedef struct {
    uint8_t R, G, B;
} led;

led matriz_led[LED_COUNT];

uint32_t valor_rgb(uint8_t B, uint8_t R, uint8_t G) {
    return (G << 24) | (R << 16) | (B << 8);
}

void set_led(uint8_t indice, uint8_t r, uint8_t g, uint8_t b) {
    if (indice < LED_COUNT) {
        matriz_led[indice] = (led){r, g, b};
    }
}

void clear_leds() {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        matriz_led[i] = (led){0, 0, 0};
    }
}

void print_leds(PIO pio, uint sm) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        pio_sm_put_blocking(pio, sm, valor_rgb(matriz_led[i].B, matriz_led[i].R, matriz_led[i].G));
    }
}

// Rotina para que seja possível acender os leds e formar os números, com base no formato do display de 7 segmentos.
void num(uint8_t value, PIO pio, uint sm) {
    static const uint8_t segmentos[10][15] = {
        {23, 22, 21, 16, 13, 6, 3, 2, 1, 8, 11, 18, 0},     //0
        {21, 18, 17, 11, 8, 1, 0},                      //1
        {1, 2, 3, 6, 12, 18, 21, 22, 23, 0},                //2
        {1, 2, 3, 8, 11, 12, 13, 18, 21, 22, 23, 0},        //3
        {1, 8, 11, 12, 13, 16, 23, 21, 18, 0},              //4
        {21, 22, 23, 16, 13, 12, 11, 8, 1, 2, 3, 0},        //5
        {21, 22, 23, 16, 13, 12, 11, 8, 1, 2, 3, 6, 0},     //6
        {23, 22, 21, 8, 11, 18, 1, 0},                      //7
        {23, 22, 21, 16, 13, 6, 3, 2, 1, 8, 11, 18, 12, 0}, //8
        {23, 22, 21, 16, 13, 1, 8, 11, 18, 12, 0}           //9
    };

    clear_leds();
    for (int i = 0; segmentos[value][i] != 0; i++) {
        set_led(segmentos[value][i], 110, 0, 0);
    }
    print_leds(pio, sm);
}



void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t agora = to_ms_since_boot(get_absolute_time());
    if (agora - tmp_ant > 200) {  // Debounce de 200 ms
        if (gpio == BTN_A && contador < 9) {
            contador++;
        } else if (gpio == BTN_B && contador > 0) {
            contador--;
        }
        tmp_ant = agora;
    }
}

// Inicialização do hardware para implementar que seja possível implementar as rotinas.
void init_hardware() {
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(BTN_A);
    gpio_init(BTN_B);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_pull_up(BTN_B);
}

int main() {
    PIO pio = pio0;
    stdio_init_all();
    init_hardware();

    uint offset = pio_add_program(pio, &ws2812b_program);
    uint sm = pio_claim_unused_sm(pio, true);
    ws2812b_program_init(pio, sm, offset, LED_PIN);

    
    
    // Configura as interrupções para que seja possível fazer as rotinas de callback
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    
    uint8_t est = 0;

    while (true) {
        num(contador, pio, sm);
        est = !est;
        gpio_put(LED_RED, est);
        sleep_ms(200);
         // Adicionado para garantir que a CPU tenha tempo de processar as interrupções
    }
}
