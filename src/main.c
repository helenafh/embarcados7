#include <kernel.h>
#include <zephyr.h>

#define PADEIRO_STACK_SIZE 1024
#define CLIENTE_STACK_SIZE 1024

#define PADEIRO_PRIORITY 5
#define CLIENTE_PRIORITY 5

#define TEMPO_PRODUCAO_MS 1000
#define TEMPO_CONSUMO_MS 1500
#define ATRASO_OPERACAO_MS 0
#define CAPACIDADE_VITRINE 10

/*
 * Selecione a parte da atividade:
 * 1: sem sincronizacao
 * 2: com mutex
 * 3: com semaforos
 */
#define MODO_ATIVIDADE 3

#if MODO_ATIVIDADE < 1 || MODO_ATIVIDADE > 3
#error "MODO_ATIVIDADE deve ser 1, 2 ou 3"
#endif

static int saldo_vitrine;

#if MODO_ATIVIDADE == 2 || MODO_ATIVIDADE == 3
K_MUTEX_DEFINE(vitrine_mutex);
#endif

#if MODO_ATIVIDADE == 3
K_SEM_DEFINE(paes_disponiveis, 0, CAPACIDADE_VITRINE);
K_SEM_DEFINE(espacos_livres, CAPACIDADE_VITRINE, CAPACIDADE_VITRINE);
#endif

static void atraso_operacao(void)
{
    if (ATRASO_OPERACAO_MS > 0) {
        k_sleep(K_MSEC(ATRASO_OPERACAO_MS));
    }
}

static const char *modo_descricao(void)
{
#if MODO_ATIVIDADE == 1
    return "Parte 1 - sem sincronizacao";
#elif MODO_ATIVIDADE == 2
    return "Parte 2 - com mutex";
#else
    return "Parte 3 - com semaforos";
#endif
}

#if MODO_ATIVIDADE == 1
static void produzir_sem_sincronizacao(void)
{
    int saldo_lido = saldo_vitrine;

    atraso_operacao();
    saldo_vitrine = saldo_lido + 1;

    printk("Padeiro produziu 1 pao. Saldo: %d -> %d\n",
           saldo_lido, saldo_vitrine);
}

static void consumir_sem_sincronizacao(void)
{
    int saldo_lido = saldo_vitrine;

    atraso_operacao();
    saldo_vitrine = saldo_lido - 1;

    printk("Cliente retirou 1 pao. Saldo: %d -> %d\n",
           saldo_lido, saldo_vitrine);
}
#endif

#if MODO_ATIVIDADE == 2
static void produzir_com_mutex(void)
{
    k_mutex_lock(&vitrine_mutex, K_FOREVER);

    int saldo_lido = saldo_vitrine;

    atraso_operacao();
    saldo_vitrine = saldo_lido + 1;

    printk("Padeiro produziu 1 pao. Saldo: %d -> %d\n",
           saldo_lido, saldo_vitrine);

    k_mutex_unlock(&vitrine_mutex);
}

static void consumir_com_mutex(void)
{
    k_mutex_lock(&vitrine_mutex, K_FOREVER);

    int saldo_lido = saldo_vitrine;

    atraso_operacao();
    saldo_vitrine = saldo_lido - 1;

    printk("Cliente retirou 1 pao. Saldo: %d -> %d\n",
           saldo_lido, saldo_vitrine);

    k_mutex_unlock(&vitrine_mutex);
}
#endif

#if MODO_ATIVIDADE == 3
static void produzir_com_semaforos(void)
{
    k_sem_take(&espacos_livres, K_FOREVER);
    k_mutex_lock(&vitrine_mutex, K_FOREVER);

    int saldo_lido = saldo_vitrine;

    atraso_operacao();
    saldo_vitrine = saldo_lido + 1;

    printk("Padeiro produziu 1 pao. Saldo: %d -> %d\n",
           saldo_lido, saldo_vitrine);

    k_mutex_unlock(&vitrine_mutex);
    k_sem_give(&paes_disponiveis);
}

static void consumir_com_semaforos(void)
{
    k_sem_take(&paes_disponiveis, K_FOREVER);
    k_mutex_lock(&vitrine_mutex, K_FOREVER);

    int saldo_lido = saldo_vitrine;

    atraso_operacao();
    saldo_vitrine = saldo_lido - 1;

    printk("Cliente retirou 1 pao. Saldo: %d -> %d\n",
           saldo_lido, saldo_vitrine);

    k_mutex_unlock(&vitrine_mutex);
    k_sem_give(&espacos_livres);
}
#endif

static void padeiro_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (1) {
#if MODO_ATIVIDADE == 1
        produzir_sem_sincronizacao();
#elif MODO_ATIVIDADE == 2
        produzir_com_mutex();
#else
        produzir_com_semaforos();
#endif

        k_sleep(K_MSEC(TEMPO_PRODUCAO_MS));
    }
}

static void cliente_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (1) {
#if MODO_ATIVIDADE == 1
        consumir_sem_sincronizacao();
#elif MODO_ATIVIDADE == 2
        consumir_com_mutex();
#else
        consumir_com_semaforos();
#endif

        k_sleep(K_MSEC(TEMPO_CONSUMO_MS));
    }
}

K_THREAD_DEFINE(padeiro_thread_id, PADEIRO_STACK_SIZE, padeiro_thread,
                NULL, NULL, NULL, PADEIRO_PRIORITY, 0, 0);

K_THREAD_DEFINE(cliente_thread_id, CLIENTE_STACK_SIZE, cliente_thread,
                NULL, NULL, NULL, CLIENTE_PRIORITY, 0, 0);

void main(void)
{
    printk("Atividade Padaria - %s\n", modo_descricao());
    printk("Padeiro: produz a cada %d ms | Cliente: consome a cada %d ms\n",
           TEMPO_PRODUCAO_MS, TEMPO_CONSUMO_MS);
    printk("Atraso artificial da operacao: %d ms\n", ATRASO_OPERACAO_MS);
    printk("Capacidade da vitrine: %d paes\n", CAPACIDADE_VITRINE);
    printk("Prioridades: padeiro=%d, cliente=%d\n",
           PADEIRO_PRIORITY, CLIENTE_PRIORITY);
}
