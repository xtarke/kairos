# Aplicação desenvolvida com ESP 8266 RTOS SDK (idf)

- ESP touch para configuração da WIFI
- menuconfig:
    - configuração do host name
    - endereço do Broker MQTT
- Logs no UART 1, baudrate 115200
- Habilitação do sensores:
    - Nos 10s incicais ao boot (9600 baudrate):
        - 'w' para reset do wifi
        - '0' liga/desliga sensor 0
        - '1' liga/desliga sensor 1
        - '2' liga/desliga sensor 2
        - '3' liga/desliga sensor 3
        - 'p' liga/desliga sensor de pressão
