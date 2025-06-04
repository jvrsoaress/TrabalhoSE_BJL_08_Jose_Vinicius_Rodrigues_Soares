<img width=100% src="https://capsule-render.vercel.app/api?type=waving&color=02A6F4&height=120&section=header"/>
<h1 align="center">Embarcatech - Projeto Integrado - BitDogLab </h1>

## Objetivo do Projeto

Um sistema de automação residencial utilizando o Raspberry Pi Pico W na plaquinha BitDogLab, permitindo o controle remoto de LEDs RGB e uma matriz WS2812 dividida em 4 cômodos (Quarto 1, Quarto 2, Cozinha, Banheiro) via protocolo MQTT, o monitoramento de temperatura em tempo real, e o gerenciamento de alarmes de emergência quando a temperatura exceder 40°C, com sinais visuais (LEDs e matriz) e sonoros (buzzer), simulando uma casa inteligente. O sistema se integra ao IoT MQTT Panel para visualização e controle, com o broker Mosquitto rodando em um celular Android via Termux.

## 🗒️ Lista de requisitos

- **Leitura de botões (A, B e Joystick):** Botão A (alterna cômodos ou desliga LEDs com pressão longa), Botão B (desliga emergência) e Joystick (muda cor).;
- **Utilização da matriz de LEDs:** Divide a matriz WS2812 em 4 cômodos (4 LEDs cada) e uma cruz central (9 LEDs brancos fixos). Ademais, exibe a cor selecionada no cômodo atual ou vermelho em emergências.;
- **Utilização de LED RGB:** Sinaliza cores em sincronia com a matriz;
- **Display OLED (SSD1306):** Exibe cômodo atual, temperatura, estado da emergência e endereço IP;
- **Utilização do buzzer:** Emite som intermitente em emergências;
- **Sensor de temperatura:** Monitora temperatura via ADC, ativando emergência acima de 40°C;
- **Protocolo MQTT:** Controle remoto via Wi-Fi com tópicos para cômodos, LEDs, cores, alarme e temperatura, além de estados publicados para monitoramento;
- **Estruturação do projeto:** Código em C no VS Code, usando Pico SDK e lwIP, com comentários detalhados;
- **Técnicas implementadas:** Wi-Fi, ADC, UART, I2C, PIO, MQTT, e debounce via software.
  

## 🛠 Tecnologias

1. **Microcontrolador:** Raspberry Pi Pico W (na BitDogLab).
2. **Display OLED SSD1306:** 128x64 pixels, conectado via I2C (GPIO 14 - SDA, GPIO 15 - SCL).
3. **Botão do Joystick:** GPIO 22 (muda cor).
4. **Botão A:** GPIO 5 (alterna cômodos/desliga LEDs).
5. **Botão B:** GPIO 6 (desliga emergência)
6. **Matriz de LEDs:** WS2812 (GPIO 7).
7. **LED RGB:** GPIOs 11 (verde), 12 (azul), 13 (vermelho).
8. **Buzzer:** GPIO 10.
9. **Sensor de temperatura:** ADC canal 4 (sensor interno do RP2040).
10. **Linguagem de Programação:** C.
11. **Frameworks:** Pico SDK, lwIP (para MQTT).
12. **Broker MQTT:** Mosquitto rodando em celular Android via Termux.
13. **Interface de Controle:** IoT MQTT Panel no Android.


## 🔧 Funcionalidades Implementadas:

**Funções dos Componentes**

- **Matriz de LEDs (WS2812):** Divide a matriz em 4 cômodos (4 LEDs cada) e uma cruz central (9 LEDs brancos fixos). Exibe a cor selecionada no cômodo atual ou vermelho durante emergências.
- **LED RGB:** Sinaliza a cor atual em sincronia com a matriz.  
- **Display OLED:** Exibe em tempo real:
  - Cômodo atual.
  - Temperatura.
  - Estado da emergência.
  - Endereço IP para conexão.
- **Buzzer:** Emite beeps intermitentes (1s ligado, 1s desligado) em emergências.
- **Botões:** 
  - Joystick: Alterna entre as 6 cores com debounce de 200ms.
  - Botão A: Alterna cômodos (pressão curta <3s) ou desliga LEDs (pressão longa ≥3s).
  - Botão B: Desliga o alarme de emergência.
- **Sensor de temperatura:** Lê o sensor interno do RP2040 a cada 1s via ADC, ativando emergência se a temperatura exceder 40°C.
- **MQTT:**
  - **Tópicos de comando:**: 
    - **casa/comando/led**: Liga/desliga LEDs ("On"/"Off").
    - **casa/comando/cor**: Seleciona cor ("Vermelho", "Verde", "Azul", "Amarelo", "Ciano", "Lilas").
    - **casa/comando/comodo**: Seleciona cômodo ("Quarto1", "Quarto2", "Cozinha", "Banheiro").
    - **casa/comando/alarme**: Desliga alarme ("Off").
  - **Tópicos de estado:**: 
    - **casa/estado/led**: Estado do LED ("LIGADO"/"DESLIGADO").
    - **casa/estado/cor**: Cor atual.
    - **casa/estado/comodo**: Cômodo atual.
    - **casa/estado/emergencia**: Estado da emergência ("LIGADA"/"DESLIGADA").
    - **casa/temperatura**: Temperatura atual (exp: "37.50").
  - Estados são publicados a cada 5s ou após mudanças.
  
- **Técnicas:**
  - Usa polling (verificação a cada 10ms) para botões, com debounce via sleep_ms(200), garantindo estabilidade sem interrupções de hardware.
  - Wi-Fi via lwIP, ADC para temperatura, UART para logs, I2C para OLED, PIO para WS2812, e MQTT para comunicação.

## 🚀 Passos para Compilação e Upload do projeto Ohmímetro com Matriz de LEDs

1. **Instale o Ambiente**:
- Configure o Pico SDK com bibliotecas lwIP.
- Instale o Mosquitto no Termux (Android):
   ```bash
  pkg install mosquitto
  mosquitto -d

- Configure o IoT MQTT Panel com o IP do broker (exp: 192.168.0.103) e tópicos.
  
2. **Compile o Código**:
- **Crie uma pasta `build`**:
   
   ```bash
   mkdir build
   cd build
   cmake ..
   make

3. **Transferir o firmware para a placa:**

- Conectar a placa BitDogLab ao computador via USB.
- Copiar o arquivo .uf2 gerado para o drive da placa.

4. **Testar o projeto**

- Após o upload, verifique o IP no Serial Monitor ou OLED.
- No IoT MQTT Panel, configure botões e gráficos para os tópicos mencionados.
- Use MQTT Explorer no PC para monitorar tópicos.
- Interaja via botões locais ou MQTT Panel.

🛠🔧🛠🔧🛠🔧


## 🎥 Demonstração: 

- Para ver o funcionamento do projeto, acesse o vídeo de demonstração gravado por José Vinicius em:

## 💻 Desenvolvedor
 
<table>
  <tr>
    <td align="center"><img style="" src="https://avatars.githubusercontent.com/u/191687774?v=4" width="100px;" alt=""/><br /><sub><b> José Vinicius </b></sub></a><br />👨‍💻</a></td>
  </tr>
</table>
