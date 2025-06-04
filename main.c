// Painel de Automação Residencial Inteligente - Adaptado p/ MQTT
// Controla LEDs RGB e matriz WS2812 dividida em 4 cômodos
// Desenvolvido por José Vinicius

#include <stdio.h>                      // biblioteca padrão para entrada/saída
#include <string.h>                     // manipulação de strings
#include <stdlib.h>                     // funções padrão
#include "pico/stdlib.h"                // funções básicas do pico sdk
#include "hardware/gpio.h"              // controle de GPIOs
#include "hardware/i2c.h"               // comunicação I2C para o display oled
#include "hardware/adc.h"               // leitura do adc para sensor de temperatura interno
#include "pico/cyw43_arch.h"            // suporte ao módulo Wi-Fi CYW43439 
#include "lwip/apps/mqtt.h"             // protocolo mqtt para comunicação IOT
#include "generated/ws2812.pio.h"      // controlar matriz WS2812 via PIO
#include "lib/ssd1306.h"               // biblioteca para display OLED SSD1306 

// credenciais Wi-Fi
#define WIFI_SSID "Apartamento 01"     // ssid (nome) da rede Wi-Fi para conexão
#define WIFI_PASSWORD "12345678"       // senha da rede Wi-Fi para autenticação

// endereço do broker mqtt
#define MQTT_BROKER_IP "192.168.0.103"  // ip do broker MQTT

// definições de pinos
#define BUTTON_A 5                     // gpio para botão A (alterna cômodos ou desliga LEDs com pressão longa)
#define BUTTON_B 6                     // GPIO para Botão B (desliga emergência)
#define WS2812_PIN 7                   // GPIO para matriz de LEDs WS2812
#define BUZZER 10                      // GPIO para buzzer (alarme de emergência)
#define LED_G 11                       // GPIO do LED RGB verde
#define LED_B 12                       // GPIO do LED RGB azul
#define LED_R 13                       // GPIO do LED RGB vermelho
#define I2C_SDA 14                     // GPIO para pino SDA do I2C (OLED)
#define I2C_SCL 15                     // GPIO para pino SCL do I2C (OLED)  
#define I2C_PORT i2c1                  // porta I2C usada para o display OLED
#define OLED_ADDRESS 0x3C              // endereço I2C do display OLED SSD1306
#define JOYSTICK 22                    // GPIO para botão do joystick (muda cor)
#define WIDTH 128                      // largura do display OLED 
#define HEIGHT 64                      // altura do display OLED 

// variáveis globais
typedef enum { VERMELHO, VERDE, AZUL, AMARELO, CIANO, LILAS } Cor; // enum para representar cores do LED RGB e matriz
typedef enum { QUARTO_1, QUARTO_2, COZINHA, BANHEIRO } Comodo;   // enum para representar os 4 cômodos controlados
static Cor cor_atual = VERMELHO;       // cor inicial do LED RGB e matriz (
static Comodo comodo_atual = QUARTO_1;// cômodo inicial 
static bool led_ligado = false;        // estado inicial do LED RGB e matriz (desligado)
static bool emergencia = false;        // estado inicial do modo de emergência (desativado)
static ssd1306_t disp;                 // estrutura para controlar o display OLED 
static uint32_t ultima_leitura_temperatura = 0; // timestamp da última leitura de temperatura
static uint32_t ultimo_botao = 0;      // timestamp da última verificação de botões
static uint32_t ultima_atualizacao_oled = 0; // timestamp da última atualização do OLED
static uint32_t ultimo_buzzer = 0;     // timestamp da última alternância do buzzer
static uint32_t botao_a_pressao_inicio = 0; // timestamp do início da pressão do botão A
static bool botao_a_pressionado = false; // estado do botão A 
static uint32_t ultima_publicacao_estado = 0; // timestamp da última publicação de estados mqtt

// mapeamento da matriz de LEDS
static const int pixel_map[5][5] = {   // índices dos LEDs na matriz 
    {24, 23, 22, 21, 20},            // Linha 1: índices dos LEDs
    {15, 16, 17, 18, 19},            // Linha 2: índices dos LEDs
    {14, 13, 12, 11, 10},            // Linha 3: índices dos LEDs
    {5,  6,  7,  8,  9},             // Linha 4: índices dos LEDs
    {4,  3,  2,  1,  0}              // Linha 5: índices dos LEDs
}; 

// LEDs por cômodo (4 LEDs por cômodo, total de 16 LEDs)
static const int comodos[4][4] = {     // mapeamento dos LEDs para cada cômodo
    {24, 23, 15, 16},                  // quarto 1: canto superior esquerdo
    {21, 20, 18, 19},                  // quarto 2: canto superior direito
    {5, 6, 4, 3},                      // cozinha: canto inferior esquerdo
    {8, 9, 1, 0}                       // banheiro: canto inferior direito
};

// LEDs da cruz central (9 LEDs, brancos fixos)
static const int cruz[] = {22, 17, 12, 7, 2, 14, 13, 11, 10}; // índices dos LEDs que formam uma cruz no centro

// estrutura para dados MQTT
typedef struct {                        // estrutura para gerenciar conexão MQTT
    mqtt_client_t *mqtt_client_inst;    // instância do cliente MQTT
    struct mqtt_connect_client_info_t mqtt_client_info; // informações de conexão (id, usuário, senha)
    ip_addr_t mqtt_server_address;      // endereço ip do broker MQTT
    char topic[32];                     // buffer para armazenar tópico atual
    bool connect_done;                  // flag para indicar conexão bem-sucedida
} MQTT_CLIENT_DATA_T;

// protótipos de funções
void inicializar_perifericos(void);     // inicializa GPIOs para LED RGB, botões, e buzzer
float ler_temperatura(void);            // lê temperatura do sensor interno via ADC
void configurar_led_rgb(Cor cor, bool estado); // configura LED RGB com cor e estado
void atualizar_matriz(void);            // atualiza matriz WS2812 com base no cômodo, cor e estado
void atualizar_display(void);           // atualiza display OLED com informações do sistema
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status); // callback de conexão MQTT
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len); // callback para tópico recebido
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags); // callback para dados recebidos
static void publish_temperature(MQTT_CLIENT_DATA_T *state); // publica temperatura no tópico MQTT
static void publish_states(MQTT_CLIENT_DATA_T *state); // publica estados dos periféricos nos tópicos MQTT

// função principal
int main() {                            // ponto de entrada do programa
    stdio_init_all();                   // inicializa uart para logs no serial monitor
    sleep_ms(2000);                     // aguarda 2 segundos para estabilizar a inicialização
    printf("Iniciando sistema de automação residencial...\n"); // loga início do sistema

    // inicializa periféricos e sensores
    inicializar_perifericos();          // configura GPIOs para LED RGB, botões, e buzzer
    adc_init();                         // inicializa módulo ADC para leitura de temperatura
    adc_set_temp_sensor_enabled(true);  // ativa sensor de temperatura interno do RP2040

    // inicializa I2C e OLED
    i2c_init(I2C_PORT, 400 * 1000);     // configura I2C a 400kHz para comunicação rápida
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // define pino SDA como função I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // define pino SCL como função I2C
    gpio_pull_up(I2C_SDA);              // habilita pull-up interno para SDA
    gpio_pull_up(I2C_SCL);              // habilita pull-up interno para SCL
    sleep_ms(500);                      // aguarda 500ms para estabilizar I2C
    ssd1306_init(&disp, WIDTH, HEIGHT, false, OLED_ADDRESS, I2C_PORT); // inicializa estrutura do OLED
    ssd1306_config(&disp);              // configura parâmetros do display OLED
    ssd1306_fill(&disp, 0);             // limpa o buffer do display
    ssd1306_send_data(&disp);           // envia buffer inicial ao OLED

    // inicializa WS2812
    PIO pio = pio0;                     // usa PIO0 para controlar a matriz WS2812
    uint offset = pio_add_program(pio, &ws2812_program); // carrega programa PIO para WS2812
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, false); // inicializa WS2812 

    // inicializa Wi-Fi
    if (cyw43_arch_init()) {            // inicializa módulo Wi-Fi CYW43439
        printf("Falha na inicialização do Wi-Fi\n"); // loga erro no Serial Monitor
        return -1;                      // encerra programa em caso de falha
    }
    cyw43_arch_enable_sta_mode();       // ativa modo estação (cliente Wi-Fi)
    printf("Conectando ao Wi-Fi...\n"); // loga tentativa de conexão
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) { // tenta conectar com timeout de 20s
        printf("Falha na conexão Wi-Fi\n"); // loga erro se a conexão falhar
        return -1;                      // encerra programa em caso de falha
    }
    printf("Conectado ao Wi-Fi\n");     // confirma conexão bem-sucedida
    if (netif_default) {                // verifica se a interface de rede está ativa
        printf("IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr)); // exibe endereço IP no Serial Monitor
    }

    static MQTT_CLIENT_DATA_T state = { // inicializa estrutura de dados MQTT
        .mqtt_client_info = {           // configura informações de conexão MQTT
            .client_id = "smart_home",  // id único do cliente MQTT
            .keep_alive = 60,           // intervalo de keep-alive em segundos
            .client_user = "Vinicius",  // usuário para autenticação no broker
            .client_pass = "Vinicius"   // senha para autenticação no broker
        }
    };
    state.mqtt_client_inst = mqtt_client_new(); // cria nova instância do cliente MQTT
    ipaddr_aton(MQTT_BROKER_IP, &state.mqtt_server_address); // converte ip do broker para formato lwip
    mqtt_client_connect(state.mqtt_client_inst, &state.mqtt_server_address, LWIP_IANA_PORT_MQTT, mqtt_connection_cb, &state, &state.mqtt_client_info); // inicia conexão MQTT
    mqtt_set_inpub_callback(state.mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, &state); // define callbacks para mensagens MQTT

    while (true) {                      // loop principal
        cyw43_arch_poll();              // processa eventos de rede (lwip) para manter MQTT ativo
        uint32_t agora = to_ms_since_boot(get_absolute_time()); // obtém tempo atual em milissegundos

        // verifica botões a cada 10ms
        if (agora - ultimo_botao >= 10) { // verifica botões a cada 10ms para responsividade
            static bool botao_joystick_pressionado = false; // estado anterior do joystick
            static bool botao_b_pressionado = false; // estado anterior do botão B

            bool estado_joystick = !gpio_get(JOYSTICK); // lê estado do joystick 
            bool estado_botao_a = !gpio_get(BUTTON_A);  // lê estado do botão A 
            bool estado_botao_b = !gpio_get(BUTTON_B);  // lê estado do botão B

            if (estado_joystick && !botao_joystick_pressionado) { // detecta nova pressão do joystick
                cor_atual = (cor_atual + 1) % 6; // cicla para a próxima cor (0 a 5)
                printf("Botão Joystick: cor alterada para %d\n", cor_atual); // loga mudança de cor
                publish_states(&state);     // publica novo estado dos periféricos
                botao_joystick_pressionado = true; // marca joystick como pressionado
                sleep_ms(200);              // debounce de 200ms para evitar múltiplas leituras
            } else if (!estado_joystick) {  // joystick liberado
                botao_joystick_pressionado = false; // reseta estado do joystick
            }

            // botão A: alterna cômodos ou desliga com pressão longa
            if (estado_botao_a && !botao_a_pressionado) { // detecta nova pressão do botão A
                botao_a_pressionado = true;    // marca botão A como pressionado
                botao_a_pressao_inicio = agora; // registra timestamp do início da pressão
                printf("Botão A: pressionado\n\n"); // loga ação
            } else if (estado_botao_a && botao_a_pressionado) { // botão A mantido pressionado
                if (agora - botao_a_pressao_inicio >= 3000) { // se pressão longa (≥3s)
                    led_ligado = false;        // desliga LEDs do cômodo
                    printf("Botão A: LEDs do cômodo desligados (pressão longa)\n\n"); // loga ação
                    publish_states(&state); // publica novo estado
                }
            } else if (!estado_botao_a && botao_a_pressionado) { // botão A liberado
                if (agora - botao_a_pressao_inicio < 3000) { // se pressão curta (<3s)
                    comodo_atual = (comodo_atual + 1) % 4; // cicla para o próximo cômodo
                    led_ligado = true;         // liga LEDs do novo cômodo
                    printf("Botão A: cômodo alterado para %d\n", comodo_atual); // loga mudança
                    publish_states(&state); // publica novo estado
                }
                botao_a_pressionado = false;   // reseta estado do botão A
                sleep_ms(200);                 // debounce de 200ms
            }

            // botão B: desliga emergência
            if (estado_botao_b && !botao_b_pressionado) { // detecta nova pressão do botão B
                emergencia = false;            // desativa modo de emergência
                printf("Botão B: alarme desligado\n\n"); // loga ação
                publish_states(&state);        // publica novo estado
                botao_b_pressionado = true;    // marca botão B como pressionado
                sleep_ms(200);                 // debounce de 200ms
            } else if (!estado_botao_b) {      // botão B liberado
                botao_b_pressionado = false;   // reseta estado do botão B
            }

            ultimo_botao = agora;              // atualiza timestamp da verificação de botões
        }

        // lê temperatura a cada 10000ms
        if (agora - ultima_leitura_temperatura >= 10000) { // lê temperatura a cada 10s
            publish_temperature(&state);    // publica temperatura no tópico MQTT
            float temperatura = ler_temperatura(); // lê temperatura do sensor interno
            if (temperatura > 40.0f) {      // se temperatura exceder 40°C
                emergencia = true;          // ativa modo de emergência
                printf("Emergência ativada: temperatura %.2f°C\n", temperatura); // loga emergência
                publish_states(&state);     // publica novo estado
            }
            ultima_leitura_temperatura = agora; // atualiza timestamp da leitura
        }

        // atualiza display a cada 1000ms
        if (agora - ultima_atualizacao_oled >= 1000) { // atualiza OLED a cada 1s
            atualizar_display();            // exibe cômodo, temperatura, emergência e ip
            ultima_atualizacao_oled = agora; // atualiza timestamp do OLED
        }

        // controla buzzer em emergência
        if (emergencia && agora - ultimo_buzzer >= 1000) { // se emergência ativa, alterna buzzer a cada 1s
            gpio_put(BUZZER, !gpio_get(BUZZER)); // inverte estado do buzzer (liga/desliga)
            ultimo_buzzer = agora;             // atualiza timestamp do buzzer
        } else if (!emergencia && gpio_get(BUZZER)) { // se emergência desativada e buzzer ligado
            gpio_put(BUZZER, 0);               // desliga buzzer
        }

        // atualiza LED RGB e matriz
        if (!emergencia) {                     // se não estiver em emergência
            configurar_led_rgb(cor_atual, led_ligado); // configura LED RGB com cor atual e estado
        } else {                               // em emergência
            configurar_led_rgb(cor_atual, false); // desliga LED RGB
        }
        atualizar_matriz();                     // atualiza matriz WS2812 (cômodo + cruz)

        if (agora - ultima_publicacao_estado >= 5000) { // publica estados a cada 5s
            publish_states(&state);         // publica estados dos periféricos
            ultima_publicacao_estado = agora; // atualiza timestamp da publicação
        }

        sleep_ms(10);                       // delay de 10ms para evitar sobrecarga do loop
    }

    cyw43_arch_deinit();                   // desinicializa wi-fi
    return 0;                              // retorno padrão
}

// inicializa periféricos
void inicializar_perifericos(void) {
    gpio_init(LED_R);                          // inicializa GPIO do LED vermelho
    gpio_set_dir(LED_R, GPIO_OUT);             // define como saída
    gpio_put(LED_R, 0);                        // desliga LED vermelho
    gpio_init(LED_G);                          // inicializa GPIO do LED verde
    gpio_set_dir(LED_G, GPIO_OUT);             // define como saída
    gpio_put(LED_G, 0);                        // desliga LED verde
    gpio_init(LED_B);                          // inicializa GPIO do LED azul
    gpio_set_dir(LED_B, GPIO_OUT);             // define como saída
    gpio_put(LED_B, 0);                        // desliga LED azul
    gpio_init(JOYSTICK);                       // inicializa GPIO do joystick
    gpio_set_dir(JOYSTICK, GPIO_IN);           // define como entrada
    gpio_pull_up(JOYSTICK);                    // habilita pull-up interno
    gpio_init(BUTTON_A);                       // inicializa GPIO do botão A
    gpio_set_dir(BUTTON_A, GPIO_IN);           // define como entrada
    gpio_pull_up(BUTTON_A);                    // habilita pull-up interno
    gpio_init(BUTTON_B);                       // inicializa GPIO do botão B
    gpio_set_dir(BUTTON_B, GPIO_IN);           // define como entrada
    gpio_pull_up(BUTTON_B);                    // habilita pull-up interno
    gpio_init(BUZZER);                         // inicializa GPIO do buzzer
    gpio_set_dir(BUZZER, GPIO_OUT);            // define como saída
    gpio_put(BUZZER, 0);                       // desliga buzzer
}

// lê temperatura do sensor interno
float ler_temperatura(void) {
    adc_select_input(4);                       // seleciona canal 4 do ADC
    uint16_t valor_bruto = adc_read();         // lê valor bruto (12 bits, 0-4095)
    const float fator_conversao = 3.3f / (1 << 12); // fator para converter ADC para tensão (Vref = 3.3V)
    float temperatura = 27.0f - ((valor_bruto * fator_conversao) - 0.706f) / 0.001721f; // converte para °C (equação do RP2040)
    return temperatura;                        // retorna temperatura em °C
}

// configura LED RGB
void configurar_led_rgb(Cor cor, bool estado) {
    uint8_t r = 0, g = 0, b = 0;              // inicializa componentes RGB como 0
    if (estado) {                              // se LED deve estar ligado
        switch (cor) {                         // define valores RGB com base na cor
            case VERMELHO: r = 32; break;     // vermelho
            case VERDE: g = 32; break;         // verde
            case AZUL: b = 32; break;          // azul
            case AMARELO: r = 32; g = 32; break; // amarelo
            case CIANO: g = 32; b = 32; break; // ciano
            case LILAS: r = 32; b = 32; break; // lilás
        }
    }
    gpio_put(LED_R, r > 0);                    // liga/desliga vermelho
    gpio_put(LED_G, g > 0);                    // liga/desliga verde
    gpio_put(LED_B, b > 0);                    // liga/desliga azul
}

// atualiza matriz de LEDs WS2812
void atualizar_matriz(void) {
    uint32_t pixels[25] = {0};                 // inicializa array de 25 LEDs como apagados
    // configura cruz branca (RGB 10, 10, 10)
    for (int i = 0; i < 9; i++) {             // itera pelos 9 LEDs da cruz
        pixels[cruz[i]] = ((uint32_t)(10) << 8) | ((uint32_t)(10) << 16) | (uint32_t)(10); // define cor branca
    }
    // configura LEDs do cômodo atual
    if (emergencia) {                          // se emergência ativa
        for (int i = 0; i < 4; i++) {         // itera pelos 4 LEDs do cômodo
            pixels[comodos[comodo_atual][i]] = ((uint32_t)(32) << 8) | ((uint32_t)(0) << 16) | (uint32_t)(0); // define cor vermelha
        }
    } else if (led_ligado) {                   // se LEDs ligados e sem emergência
        uint8_t r = 0, g = 0, b = 0;          // inicializa componentes RGB
        switch (cor_atual) {                   // define valores RGB com base na cor atual
            case VERMELHO: r = 32; break;     // vermelho
            case VERDE: g = 32; break;         // verde
            case AZUL: b = 32; break;          // azul
            case AMARELO: r = 32; g = 32; break; // amarelo
            case CIANO: g = 32; b = 32; break; // ciano
            case LILAS: r = 32; b = 32; break; // lilás
        }
        for (int i = 0; i < 4; i++) {         // itera pelos 4 LEDs do cômodo
            pixels[comodos[comodo_atual][i]] = ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b); // define cor
        }
    }
    // envia dados para a matriz WS2812
    for (int i = 0; i < 25; i++) {            // itera pelos 25 LEDs
        pio_sm_put_blocking(pio0, 0, pixels[i] << 8u); // envia valor RGB para WS2812 via PIO
    }
}

// atualiza display OLED
void atualizar_display(void) {
    char temp_str[20];                        // buffer para string do cômodo/temperatura
    char ip_str[16];                          // buffer para string do endereço IP
    ssd1306_fill(&disp, 0);                   // limpa o buffer do display
    snprintf(temp_str, sizeof(temp_str), "%s", // formata nome do cômodo atual
             comodo_atual == QUARTO_1 ? "QUARTO 1" :
             comodo_atual == QUARTO_2 ? "QUARTO 2" :
             comodo_atual == COZINHA ? "COZINHA" : "BANHEIRO");
    ssd1306_draw_string(&disp, temp_str, 20, 2); // exibe cômodo na linha 1
    snprintf(temp_str, sizeof(temp_str), "TEMP: %.1fC", ler_temperatura()); // formata temperatura
    ssd1306_draw_string(&disp, temp_str, 20, 18); // exibe temperatura na linha 2
    ssd1306_draw_string(&disp, emergencia ? "EMERGENCIA: ON" : "EMERGENCIA: OFF", 2, 34); // exibe estado da emergência na linha 3
    snprintf(ip_str, sizeof(ip_str), "%s", netif_default ? ipaddr_ntoa(&netif_default->ip_addr) : "N/A"); // formata endereço IP
    ssd1306_draw_string(&disp, ip_str, 6, 50); // exibe IP na linha 4
    ssd1306_send_data(&disp);                 // envia buffer ao display OLED
}

// callback de conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) { // gerencia conexão MQTT
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg; // converte argumento para estrutura MQTT
    if (status == MQTT_CONNECT_ACCEPTED) { // se conexão bem-sucedida
        printf("Conectado ao broker MQTT com sucesso\n"); // loga sucesso
        state->connect_done = true;        // marca conexão como concluída
        mqtt_subscribe(state->mqtt_client_inst, "casa/comando/led", 1, NULL, state); // inscreve no tópico de led
        mqtt_subscribe(state->mqtt_client_inst, "casa/comando/cor", 1, NULL, state); // inscreve no tópico de cor
        mqtt_subscribe(state->mqtt_client_inst, "casa/comando/comodo", 1, NULL, state); // inscreve no tópico de cômodo
        mqtt_subscribe(state->mqtt_client_inst, "casa/comando/alarme", 1, NULL, state); // inscreve no tópico de alarme
        printf("Inscrito nos tópicos de comando\n"); // loga inscrição
        publish_states(state);             // publica estados iniciais
    } else {                               // se conexão falhou
        printf("Falha na conexão MQTT: %d\n", status); // loga erro
    }
}

// callback para tópico recebido
static void mqtt_incoming_publish_cb(void *arg, const char *topic, uint32_t tot_len) { // processa tópico MQTT recebido
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg; // converte argumento para estado
    strncpy(state->topic, topic, sizeof(state->topic)); // copia tópico para buffer
    printf("Mensagem recebida no tópico: %s\n", topic); // loga tópico recebido
}

// callback para dados recebidos
static void mqtt_incoming_data_cb(void *arg, const uint8_t *data, uint16_t len, uint8_t flags) { // processa dados MQTT
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg; // converte argumento para estado
    char payload[32];                     // buffer para payload
    strncpy(payload, (const char*)data, len); // copia dados para buffer
    payload[len] = '\0';                  // adiciona terminador nulo
    printf("Payload recebido: %s\n", payload); // loga payload

    if (strcmp(state->topic, "casa/comando/led") == 0) { // se tópico for de comando de led
        if (strcmp(payload, "On") == 0) {     // se comando for ligar
            led_ligado = true;                // ativa led
            printf("LED ligado via MQTT\n");  // loga ação
        } else if (strcmp(payload, "Off") == 0) { // se comando for desligar
            led_ligado = false;               // desativa led
            printf("LED desligado via MQTT\n"); // loga ação
        }
        publish_states(state);               // publica novo estado
    } else if (strcmp(state->topic, "casa/comando/cor") == 0) { // se tópico for de cor
        if (strcmp(payload, "Vermelho") == 0) cor_atual = VERMELHO; // define vermelho
        else if (strcmp(payload, "Verde") == 0) cor_atual = VERDE; // define verde
        else if (strcmp(payload, "Azul") == 0) cor_atual = AZUL; // define azul
        else if (strcmp(payload, "Amarelo") == 0) cor_atual = AMARELO; // define amarelo
        else if (strcmp(payload, "Ciano") == 0) cor_atual = CIANO; // define ciano
        else if (strcmp(payload, "Lilas") == 0) cor_atual = LILAS; // define lilás
        printf("Cor alterada para %d via MQTT\n", cor_atual); // loga mudança
        publish_states(state);               // publica novo estado
    } else if (strcmp(state->topic, "casa/comando/comodo") == 0) { // se tópico for de cômodo
        if (strcmp(payload, "Quarto1") == 0) { comodo_atual = QUARTO_1; led_ligado = true; } // quarto 1
        else if (strcmp(payload, "Quarto2") == 0) { comodo_atual = QUARTO_2; led_ligado = true; } // quarto 2
        else if (strcmp(payload, "Cozinha") == 0) { comodo_atual = COZINHA; led_ligado = true; } // cozinha
        else if (strcmp(payload, "Banheiro") == 0) { comodo_atual = BANHEIRO; led_ligado = true; } // banheiro
        printf("Cômodo alterado para %d via MQTT\n", comodo_atual); // loga mudança
        publish_states(state);               // publica novo estado
    } else if (strcmp(state->topic, "casa/comando/alarme") == 0) { // se tópico for de alarme
        if (strcmp(payload, "Off") == 0) {   // se comando for desligar alarme
            emergencia = false;              // desativa emergência
            printf("Alarme desligado via MQTT\n"); // loga ação
            publish_states(state);           // publica novo estado
        }
    }
}

// publica temperatura
static void publish_temperature(MQTT_CLIENT_DATA_T *state) { // publica temperatura no tópico MQTT
    if (!state->connect_done) {           // se não conectado ao broker
        printf("Não conectado ao broker, pulando publicação de temperatura\n"); // loga aviso
        return;                           // sai da função
    }
    float temp = ler_temperatura();       // lê temperatura atual
    char temp_str[16];                    // buffer para string da temperatura
    snprintf(temp_str, sizeof(temp_str), "%.2f", temp); // formata temperatura com 2 casas decimais
    printf("Publicando temperatura: %s°C no tópico casa/temperatura\n", temp_str); // loga publicação
    mqtt_publish(state->mqtt_client_inst, "casa/temperatura", temp_str, strlen(temp_str), 1, 0, NULL, state); // publica no tópico
}

// publica estados dos periféricos
static void publish_states(MQTT_CLIENT_DATA_T *state) { // publica estados dos periféricos
    if (!state->connect_done) {           // se não conectado ao broker
        printf("Não conectado ao broker, pulando publicação de estados\n"); // loga aviso
        return;                           // sai da função
    }
    mqtt_publish(state->mqtt_client_inst, "casa/estado/led", led_ligado ? "LIGADO" : "DESLIGADO", strlen(led_ligado ? "LIGADO" : "DESLIGADO"), 1, 0, NULL, state); // publica estado do led
    const char* cor = cor_atual == VERMELHO ? "Vermelho" : // define nome da cor atual
                      cor_atual == VERDE ? "Verde" :
                      cor_atual == AZUL ? "Azul" :
                      cor_atual == AMARELO ? "Amarelo" :
                      cor_atual == CIANO ? "Ciano" : "Lilas";
    mqtt_publish(state->mqtt_client_inst, "casa/estado/cor", cor, strlen(cor), 1, 0, NULL, state); // publica cor atual
    const char* comodo = comodo_atual == QUARTO_1 ? "Quarto1" : // define nome do cômodo atual
                         comodo_atual == QUARTO_2 ? "Quarto2" :
                         comodo_atual == COZINHA ? "Cozinha" : "Banheiro";
    mqtt_publish(state->mqtt_client_inst, "casa/estado/comodo", comodo, strlen(comodo), 1, 0, NULL, state); // publica cômodo atual
    mqtt_publish(state->mqtt_client_inst, "casa/estado/emergencia", emergencia ? "LIGADA" : "DESLIGADA", strlen(emergencia ? "LIGADA" : "DESLIGADA"), 1, 0, NULL, state); // publica estado da emergência
    printf("Estados publicados: LED=%s, Cor=%s, Cômodo=%s, Emergência=%s\n", // loga estados publicados
           led_ligado ? "LIGADO" : "DESLIGADO", cor, comodo, emergencia ? "LIGADA" : "DESLIGADA");
}