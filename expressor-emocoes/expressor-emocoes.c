#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/pio.h"
#include "lib/pico-ssd1306/ssd1306.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// defines para pinos ou globais específicas
#define I2C_PORT i2c1           // Porta I2C
#define I2C_SDA 14              // I2C SDA
#define I2C_SCL 15              // I2C SCL
#define PIN_BUTTON_NEXT 6       // Pino botão de navegação
#define PIN_BUTTON_CONF 5       // Pino botão de confirmação
#define PIN_LED_RED 13          // Pino led vermelho
#define PIN_LED_BLUE 12         // Pino led verde
#define PIN_LED_GREEN 11        // Pino led azul
#define PIN_BUZZER_NAV 21       // Pino buzzer do sfx de navegação
#define DIVIDER_PWM 16.0        // Divisor de clock para o pwm
#define PIN_LED_ARRAY 7         // Pino matriz de leds RGB
#define LED_COUNT 25            // Quantidade de leds na matriz

// struct para notas musicais
typedef struct
{
    uint16_t period;   // frequência da nota
    uint64_t duration; // duração da nota
} Note;

// Definição de pixel GRB
struct pixel_t
{
    uint8_t G, R, B; // Três valores de 8 bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

//                                GLOBAIS

// músicas para cada emoção

const Note happy_song[] = {
    {1911, 150}, {1703, 150}, {1517, 150}, {1432, 150}, {1276, 300}};

const Note sad_song[] = {
    {1276, 300}, {1432, 300}, {1517, 300}, {1703, 500}};

const Note angry_song[] = {
    {1000, 100}, {900, 100}, {800, 100}, {700, 100}, {600, 200}};

const Note calm_song[] = {
    {1911, 500}, {1703, 500}, {1432, 500}, {1276, 700}};

// array com os tamanhos das músicas
const uint song_lengths[] = {
    sizeof(happy_song) / sizeof(happy_song[0]),
    sizeof(sad_song) / sizeof(sad_song[0]),
    sizeof(angry_song) / sizeof(angry_song[0]),
    sizeof(calm_song) / sizeof(calm_song[0])};

// Array de ponteiros para as músicas
const Note *songs[] = {happy_song, sad_song, angry_song, calm_song};

// arrays de leds para cada emoção
npLED_t happy_face[] = {
    {0, 0, 0}, {80, 80, 0}, {80, 80, 0}, {80, 80, 0}, {0, 0, 0}, {80, 80, 0}, {80, 80, 0}, {80, 80, 0}, {80, 80, 0}, {80, 80, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {80, 80, 0}, {0, 0, 0}, {80, 80, 0}, {0, 0, 0}, {0, 0, 0}, {80, 80, 0}, {0, 0, 0}, {80, 80, 0}, {0, 0, 0}};

npLED_t sad_face[] = {
    {0, 0, 0}, {0, 0, 80}, {0, 0, 0}, {0, 0, 80}, {0, 0, 0}, {0, 0, 0}, {0, 0, 80}, {0, 0, 80}, {0, 0, 80}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 80}, {0, 0, 80}, {0, 0, 0}, {0, 0, 80}, {0, 0, 80}, {0, 0, 0}, {0, 0, 80}, {0, 0, 0}, {0, 0, 80}, {0, 0, 0}};

npLED_t angry_face[] = {
    {0, 80, 0}, {0, 80, 0}, {0, 80, 0}, {0, 80, 0}, {0, 80, 0}, {0, 0, 0}, {0, 80, 0}, {0, 80, 0}, {0, 80, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 80, 0}, {0, 0, 0}, {0, 80, 0}, {0, 0, 0}, {0, 80, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 80, 0}};

npLED_t calm_face[] = {
    {0, 0, 0}, {80, 0, 0}, {80, 0, 0}, {80, 0, 0}, {0, 0, 0}, {80, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {80, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {80, 0, 0}, {0, 0, 0}, {80, 0, 0}, {0, 0, 0}, {0, 0, 0}, {80, 0, 0}, {0, 0, 0}, {80, 0, 0}, {0, 0, 0}};

// Array de ponteiros para os desenhos
const npLED_t *faces[] = {happy_face, sad_face, angry_face, calm_face};

const uint base_period = 2000;                                            // Período base do pwm
const uint period = base_period;                                          // Periodo do buzzer
volatile bool play_sfx = false;                                           // Flag para indicar se o som deve tocar
volatile bool play_music = false;                                         // Flag para indicar se a música deve tocar
volatile uint64_t start_time_sfx = 0;                                     // Marca o início do som
const uint64_t nav_sfx_duration = 100;                                    // Duração do som em milissegundos
volatile uint64_t last_button_press_nav = 0;                              // Marca o último evento do botão de navegação
volatile uint64_t last_button_press_conf = 0;                             // Marca o último evento do botão de confirmação
const uint64_t debounce_time_ms = 200;                                    // Tempo de debounce (200ms)
const char *emotions[] = {"Feliz:D", "Triste:( ", "Bravo>:(", "Calmo:)"}; // Array de emoções
const uint num_emotions = 4;                                              // Tamanho do array de emoções
volatile uint current_emotion = 0;                                        // Emoção atual na navegação
PIO np_pio;                                                               // Representa um periférico PIO
uint sm;                                                                  // Armazena o número da máquina de estado do PIO

//                                FUNÇÕES

// função de setup para os componente utilizados
void setup(ssd1306_t *oled)
{
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // setup leds
    gpio_init(PIN_LED_RED);
    gpio_init(PIN_LED_GREEN);
    gpio_init(PIN_LED_BLUE);
    gpio_set_dir(PIN_LED_RED, GPIO_OUT);
    gpio_set_dir(PIN_LED_GREEN, GPIO_OUT);
    gpio_set_dir(PIN_LED_BLUE, GPIO_OUT);

    // setup botoes
    gpio_init(PIN_BUTTON_NEXT);
    gpio_set_dir(PIN_BUTTON_NEXT, GPIO_IN);
    gpio_pull_up(PIN_BUTTON_NEXT);

    gpio_init(PIN_BUTTON_CONF);
    gpio_set_dir(PIN_BUTTON_CONF, GPIO_IN);
    gpio_pull_up(PIN_BUTTON_CONF);

    // setup oled
    ssd1306_init(oled, 128, 64, 0x3c, I2C_PORT);
}

void npInit(uint pin)
{
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0)
    {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
}

void npWrite()
{
    const npLED_t *leds = faces[current_emotion];

    // Escreve cada dado de 8 bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

void npClear()
{
    // Escreve cada dado de 8 bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, 0);
        pio_sm_put_blocking(np_pio, sm, 0);
        pio_sm_put_blocking(np_pio, sm, 0);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

// setup para o pwm
void setup_pwm(uint slice, uint32_t period, uint pin)
{
    pwm_set_clkdiv(slice, DIVIDER_PWM);
    pwm_set_wrap(slice, period);
    pwm_set_gpio_level(pin, period / 3.3);
    pwm_set_enabled(slice, true);
}

// define a cor do led rgb
void set_led_color(bool red, bool green, bool blue)
{
    gpio_put(PIN_LED_RED, red);
    gpio_put(PIN_LED_GREEN, green);
    gpio_put(PIN_LED_BLUE, blue);
}

// menu do display
void display_emotion(ssd1306_t *oled, const char *emotion)
{
    ssd1306_clear(oled);
    ssd1306_draw_string(oled, 0, 0, 1, "Escolha uma emocao:");
    ssd1306_draw_string(oled, 15, 18, 2, emotion);
    ssd1306_draw_string(oled, 0, 44, 1, "B - Proximo");
    ssd1306_draw_string(oled, 0, 54, 1, "A - Confirmar");
    ssd1306_show(oled);
}

// Função para tocar a música correspondente à emoção atual
void play_emotion_song(uint slice)
{
    const Note *song = songs[current_emotion];        // Obtém a música correspondente
    uint song_length = song_lengths[current_emotion]; // Obtém o tamanho da música

    npWrite(); // Desenha a figura correspondente a emoção atual

    // Toca música correspondente a emoção atual
    for (uint i = 0; i < song_length; i++)
    {
        setup_pwm(slice, song[i].period, PIN_BUZZER_NAV);
        sleep_ms(song[i].duration);
    }

    npClear(); // Limpa matriz de leds

    // Encerra música
    pwm_set_enabled(slice, false);
    pwm_set_gpio_level(PIN_BUZZER_NAV, 0);
    play_music = false;
}

// callback botão de navegação
void button_callback(uint gpio, uint32_t events)
{
    uint64_t current_time = to_ms_since_boot(get_absolute_time());

    if (gpio == PIN_BUTTON_NEXT) // Botão de navegação
    {
        printf("Botão de navegação pressionado\n"); // debug
        if (current_time - last_button_press_nav > debounce_time_ms) // verificação de debouncing
        {
            last_button_press_nav = current_time;

            // Navegar entre emoções
            current_emotion = (current_emotion + 1) % num_emotions;

            // Atualizar a cor do LED
            switch (current_emotion)
            {
            case 0:
                set_led_color(true, true, false);
                break; // Amarelo
            case 1:
                set_led_color(false, false, true);
                break; // Azul
            case 2:
                set_led_color(true, false, false);
                break; // Vermelho
            case 3:
                set_led_color(false, true, false);
                break; // Verde
            }

            // Tocar som curto de navegação
            play_sfx = true;
            start_time_sfx = current_time;
        }
    }
    else if (gpio == PIN_BUTTON_CONF) // Botão de confirmação
    {
        printf("Botão de confirmação pressionado\n"); // debug

        if (current_time - last_button_press_conf > debounce_time_ms) // verificação de debouncing
        {
            last_button_press_conf = current_time;

            // Ativar música
            play_music = true;
        }
    }
}

//                                FUNÇÃO MAIN

int main()
{
    stdio_init_all();

    // setup matriz de leds
    npInit(PIN_LED_ARRAY);

    // Configura interrupção nos botões de navegação e confirmação
    gpio_set_irq_enabled(PIN_BUTTON_CONF, GPIO_IRQ_EDGE_FALL, true);                                 // Apenas habilita a interrupção
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_NEXT, GPIO_IRQ_EDGE_FALL, true, &button_callback); // Registra o callback global

    // setups pwm
    gpio_set_function(PIN_BUZZER_NAV, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_BUZZER_NAV);

    // setup dos periféricos restantes
    ssd1306_t oled;
    oled.external_vcc = false;
    setup(&oled);

    // led rgb inicia em amarelo
    set_led_color(true, true, false);

    while (true)
    {
        // Exibe o menu de navegação
        display_emotion(&oled, emotions[current_emotion]);

        // Verifica flag para tocar o som de navegação
        if (play_sfx)
        {
            uint64_t current_time = to_ms_since_boot(get_absolute_time());
            setup_pwm(slice, period, PIN_BUZZER_NAV);

            if (current_time - start_time_sfx >= nav_sfx_duration)
            {
                play_sfx = false;
                pwm_set_enabled(slice, false);
                pwm_set_gpio_level(PIN_BUZZER_NAV, 0);
            }
        }

        // Verifica flag para tocar a música
        if (play_music)
        {
            play_emotion_song(slice); // Toca a música da emoção
        }

        tight_loop_contents();
    }
}