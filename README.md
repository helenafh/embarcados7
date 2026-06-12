# Atividade 6 - FRDM-KL25Z com Zephyr

Projeto PlatformIO/Zephyr para a placa FRDM-KL25Z integrando:

- ADC com leitura a cada 500 ms.
- Botão `SW0` com interrupção.
- Acelerômetro MMA8451Q com leitura dos eixos X, Y e Z a cada 1000 ms.
- Alternância de modo por botão.
- Experimento com prioridades de threads.

## Modos de operação

O sistema inicia em `Modo ADC`, exibindo apenas as leituras do ADC na serial.

A cada pressionamento do botão `SW0`, o callback da interrupção alterna o modo:

- `Modo ADC`: imprime somente o ADC.
- `Modo Completo`: imprime ADC e acelerômetro.

Foi usado um debounce simples de 200 ms para evitar múltiplas alternâncias em um único pressionamento.

## Threads

A thread do ADC executa:

- `adc_read()`
- conversão para mV com `adc_raw_to_millivolts()`
- `k_sleep(K_MSEC(500))`

A thread do acelerômetro executa:

- `sensor_sample_fetch()`
- `sensor_channel_get(..., SENSOR_CHAN_ACCEL_XYZ, ...)`
- `k_sleep(K_MSEC(1000))`

O acelerômetro só imprime no `Modo Completo`.

## Experimento de prioridades

No Zephyr, números menores indicam prioridades maiores para threads preemptivas. Assim, prioridade `4` executa antes de prioridade `5` quando ambas estão prontas ao mesmo tempo.

O arquivo `src/main.c` possui a macro:

```c
#define PRIORITY_EXPERIMENT 1
```

Use os valores:

- `0`: ADC e acelerômetro com a mesma prioridade.
- `1`: ADC com maior prioridade.
- `2`: acelerômetro com maior prioridade.

Com prioridades iguais, quando as duas threads acordam próximas, o Zephyr as agenda de forma justa dentro da mesma prioridade. Como as duas passam quase todo o tempo dormindo, a diferença prática na serial é pequena.

Com `PRIORITY_EXPERIMENT 1`, o ADC tem prioridade maior. Quando ADC e acelerômetro ficam prontos no mesmo instante, o ADC tende a imprimir primeiro.

Com `PRIORITY_EXPERIMENT 2`, o acelerômetro tem prioridade maior. No modo completo, quando as duas threads acordam juntas, a leitura do acelerômetro tende a ocorrer antes da leitura do ADC.

Como as operações são curtas e ambas usam `k_sleep()`, a alteração de prioridade não muda significativamente os períodos de 500 ms e 1000 ms. Ela altera principalmente a ordem de execução nos instantes em que as threads ficam prontas simultaneamente.

## Build

```sh
/home/helenafh/.platformio/penv/bin/pio run
```
