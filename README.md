# Atividade 7 - Produtor e Consumidor: Padaria

Projeto PlatformIO/Zephyr para a placa FRDM-KL25Z.

Esta atividade simula uma padaria com duas threads:

- Padeiro: produz paes e incrementa `saldo_vitrine`.
- Cliente: retira paes e decrementa `saldo_vitrine`.

A variavel `saldo_vitrine` representa a quantidade de paes disponiveis na
vitrine.

## Estrutura inicial

O arquivo `src/main.c` foi limpo da atividade anterior e agora contem somente a
base da simulacao:

- `padeiro_thread`: produz 1 pao a cada 1000 ms.
- `cliente_thread`: consome 1 pao a cada 1500 ms.
- `saldo_vitrine`: recurso compartilhado entre as duas threads.
- `printk`: imprime os eventos e o saldo atual na serial.

Nesta etapa inicial ainda nao ha mutexes, semaforos ou filas. Isso permite
observar o comportamento da Parte 1 da atividade.

## Selecao da parte

O arquivo `src/main.c` possui a macro:

```c
#define MODO_ATIVIDADE 3
```

Use os valores:

- `1`: Parte 1, sem sincronizacao.
- `2`: Parte 2, com mutex.
- `3`: Parte 3, com semaforos.

As tres partes da atividade estao implementadas.

## Parte 1 - sem sincronizacao

Com `MODO_ATIVIDADE` igual a `1`, o programa executa a versao sem mutexes,
semaforos ou filas.

As macros de experimento ficam no inicio de `src/main.c`:

```c
#define PADEIRO_PRIORITY 5
#define CLIENTE_PRIORITY 5

#define TEMPO_PRODUCAO_MS 1000
#define TEMPO_CONSUMO_MS 1500
#define ATRASO_OPERACAO_MS 0
#define CAPACIDADE_VITRINE 10
```

Para observar mudancas de comportamento, altere:

- `TEMPO_PRODUCAO_MS`: intervalo entre producoes do padeiro.
- `TEMPO_CONSUMO_MS`: intervalo entre retiradas do cliente.
- `PADEIRO_PRIORITY` e `CLIENTE_PRIORITY`: prioridades das threads. No Zephyr,
  numeros menores indicam prioridade maior.
- `ATRASO_OPERACAO_MS`: atraso artificial entre ler e escrever
  `saldo_vitrine`, util para evidenciar acessos intercalados.

Nesta parte, `saldo_vitrine` e acessada diretamente pelas duas threads. Isso
permite observar condicoes de corrida e tambem permite que o cliente retire pao
mesmo quando a vitrine esta vazia.

## Parte 2 - com mutex

Com `MODO_ATIVIDADE` igual a `2`, o programa utiliza um mutex para proteger o
acesso a `saldo_vitrine`.

O mutex usado no codigo e:

```c
K_MUTEX_DEFINE(vitrine_mutex);
```

O padeiro e o cliente executam suas alteracoes dentro de uma regiao critica:

```c
k_mutex_lock(&vitrine_mutex, K_FOREVER);
/* leitura e escrita de saldo_vitrine */
k_mutex_unlock(&vitrine_mutex);
```

Nesta parte, o mutex garante que apenas uma thread por vez leia e escreva
`saldo_vitrine`. Isso evita atualizacoes intercaladas sobre a variavel
compartilhada.

O mutex nao controla a disponibilidade de paes. Portanto, ele nao impede que o
cliente retire pao quando `saldo_vitrine` esta em zero. Essa regra sera tratada
na Parte 3 com semaforos.

## Parte 3 - com semaforos

Com `MODO_ATIVIDADE` igual a `3`, o programa utiliza semaforos para controlar a
disponibilidade da vitrine, que possui capacidade maxima de 10 paes.

Os semaforos usados no codigo sao:

```c
K_SEM_DEFINE(paes_disponiveis, 0, CAPACIDADE_VITRINE);
K_SEM_DEFINE(espacos_livres, CAPACIDADE_VITRINE, CAPACIDADE_VITRINE);
```

Papel de cada semaforo:

- `paes_disponiveis`: conta quantos paes podem ser retirados. O cliente espera
  nesse semaforo antes de consumir.
- `espacos_livres`: conta quantos espacos ainda existem na vitrine. O padeiro
  espera nesse semaforo antes de produzir.

Fluxo do padeiro:

```c
k_sem_take(&espacos_livres, K_FOREVER);
/* incrementa saldo_vitrine */
k_sem_give(&paes_disponiveis);
```

Fluxo do cliente:

```c
k_sem_take(&paes_disponiveis, K_FOREVER);
/* decrementa saldo_vitrine */
k_sem_give(&espacos_livres);
```

Nesta parte, tambem e usado `vitrine_mutex` durante a leitura e escrita de
`saldo_vitrine`. Os semaforos controlam quando produzir ou consumir; o mutex
protege a atualizacao da variavel compartilhada.

Com isso, o saldo nao fica negativo e tambem nao ultrapassa
`CAPACIDADE_VITRINE`.

## Build

```sh
/home/helenafh/.platformio/penv/bin/pio run
```
