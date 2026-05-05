#include <zephyr.h>             // Funções básicas do Zephyr (ex: k_msleep)
#include <device.h>             // API  para obter e usar dispositivos
#include <drivers/gpio.h>       // API para controle de GPIO

#define LED_PORT       "GPIO_1" // Nome do controlador GPIO (label no .pio\build\frdm_kl25z\zephyr\zephyr.dts)
#define LED_PIN_GREEN        19       // Pino PTB19 onde está o LED verde
#define SLEEP_TIME_MS  500    // Intervalo de piscar (milissegundos)

void main(void)
{
    const struct device *port = device_get_binding(LED_PORT);
    // Obtém ponteiro para o controlador GPIO “GPIO_1”
    // (não é feito a cada iteração para economizar chamadas)

    gpio_pin_configure(port, LED_PIN_GREEN, GPIO_OUTPUT_ACTIVE);
    // Configura o pino como saída ativa (LED apagado ou aceso depende de pull)

    while (1) {
        gpio_pin_toggle(port, LED_PIN_GREEN);
        // Inverte o nível no pino (se estava alto, passa para baixo e vice-versa)

        k_msleep(SLEEP_TIME_MS);
        // Suspende a tarefa por SLEEP_TIME_MS milissegundos antes do próximo toggle
    }
}