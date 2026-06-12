#include <errno.h>
#include <zephyr.h>             // Funções básicas do Zephyr (ex: k_msleep)
#include <device.h>             // API  para obter e usar dispositivos
#include <kernel.h>
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

//ADC
#define ADC_RESOLUTION       12
#define ADC_GAIN             ADC_GAIN_1
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#define ADC_CHANNEL_ID       0  //Canal do ADC, veja a pinagem
#define ADC_VREF_MV          3300

#define ADC_THREAD_STACK_SIZE 1024

#define ACCEL_DEVICE_LABEL        "MMA8451Q"
#define ACCEL_THREAD_STACK_SIZE   1024

/*
 * Experimentos de prioridade:
 * 0: ADC e acelerômetro com a mesma prioridade
 * 1: ADC com maior prioridade
 * 2: acelerômetro com maior prioridade
 */
#define PRIORITY_EXPERIMENT 2

#if PRIORITY_EXPERIMENT == 0
#define ADC_THREAD_PRIORITY   5
#define ACCEL_THREAD_PRIORITY 5
#elif PRIORITY_EXPERIMENT == 1
#define ADC_THREAD_PRIORITY   4
#define ACCEL_THREAD_PRIORITY 5
#elif PRIORITY_EXPERIMENT == 2
#define ADC_THREAD_PRIORITY   5
#define ACCEL_THREAD_PRIORITY 4
#else
#error "PRIORITY_EXPERIMENT deve ser 0, 1 ou 2"
#endif

#define BUTTON_NODE DT_ALIAS(sw0)
#define BUTTON_DEBOUNCE_MS 200

static int16_t sample_buffer;
static const struct device *adc_dev;
static const struct device *accel_dev;
static volatile bool complete_mode;
static int64_t last_button_time;
static struct gpio_callback button_cb_data;
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static struct adc_sequence adc_sequence = {
    .channels    = BIT(ADC_CHANNEL_ID),
    .buffer      = &sample_buffer,
    .buffer_size = sizeof(sample_buffer),
    .resolution  = ADC_RESOLUTION,
};

K_SEM_DEFINE(adc_ready_sem, 0, 1);
K_SEM_DEFINE(accel_ready_sem, 0, 1);

static void adc_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    k_sem_take(&adc_ready_sem, K_FOREVER);

    while (1) {
        int err = adc_read(adc_dev, &adc_sequence);

        if (err != 0) {
            printk("Falha na leitura do ADC: %d\n", err);
        } else {
            int32_t mv = sample_buffer;

            adc_raw_to_millivolts(ADC_VREF_MV, ADC_GAIN, ADC_RESOLUTION, &mv);
            printk("ADC: %d (raw), %d mV\n", sample_buffer, mv);
        }

        k_sleep(K_MSEC(500));
    }
}

K_THREAD_DEFINE(adc_thread_id, ADC_THREAD_STACK_SIZE, adc_thread,
                NULL, NULL, NULL, ADC_THREAD_PRIORITY, 0, 0);

static void accel_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    k_sem_take(&accel_ready_sem, K_FOREVER);

    while (1) {
        if (!complete_mode) {
            k_sleep(K_MSEC(1000));
            continue;
        }

        struct sensor_value accel[3];
        int err = sensor_sample_fetch(accel_dev);

        if (err != 0) {
            printk("Falha na leitura do acelerômetro: %d\n", err);
        } else {
            err = sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
            if (err != 0) {
                printk("Falha ao obter eixos do acelerômetro: %d\n", err);
            } else {
                printk("ACCEL: X=%d.%06d, Y=%d.%06d, Z=%d.%06d m/s^2\n",
                       accel[0].val1, accel[0].val2,
                       accel[1].val1, accel[1].val2,
                       accel[2].val1, accel[2].val2);
            }
        }

        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(accel_thread_id, ACCEL_THREAD_STACK_SIZE, accel_thread,
                NULL, NULL, NULL, ACCEL_THREAD_PRIORITY, 0, 0);

static void button_pressed(const struct device *dev,
                           struct gpio_callback *cb,
                           uint32_t pins)
{
    int64_t now = k_uptime_get();

    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    if (now - last_button_time < BUTTON_DEBOUNCE_MS) {
        return;
    }

    last_button_time = now;
    complete_mode = !complete_mode;

    printk("Modo alterado: %s\n", complete_mode ? "Completo" : "ADC");
}

static int configure_button(void)
{
    int err;

    if (!device_is_ready(button.port)) {
        printk("Botão não está pronto\n");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err != 0) {
        printk("Erro ao configurar botão: %d\n", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err != 0) {
        printk("Erro ao configurar interrupção do botão: %d\n", err);
        return err;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    err = gpio_add_callback(button.port, &button_cb_data);
    if (err != 0) {
        printk("Erro ao registrar callback do botão: %d\n", err);
        return err;
    }

    printk("Botão configurado. Modo inicial: ADC.\n");
    return 0;
}


void main(void)
{
    if (configure_button() != 0) {
        return;
    }

    printk("Prioridades: ADC=%d, acelerômetro=%d\n",
           ADC_THREAD_PRIORITY, ACCEL_THREAD_PRIORITY);

    //ADC
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));
    if (!device_is_ready(adc_dev)) {
        printk("ADC não está pronto\n");
        return;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN,
        .reference = ADC_REFERENCE,
        .acquisition_time = ADC_ACQUISITION_TIME,
        .channel_id = ADC_CHANNEL_ID,
        .differential = 0,
    };

    if (adc_channel_setup(adc_dev, &channel_cfg) != 0) {
        printk("Erro ao configurar canal ADC\n");
        return;
    }

    printk("ADC configurado. Thread lendo a cada 500 ms.\n");
    k_sem_give(&adc_ready_sem);

    accel_dev = device_get_binding(ACCEL_DEVICE_LABEL);
    if (accel_dev == NULL) {
        printk("Acelerômetro %s não encontrado\n", ACCEL_DEVICE_LABEL);
        return;
    }

    if (!device_is_ready(accel_dev)) {
        printk("Acelerômetro %s não está pronto\n", ACCEL_DEVICE_LABEL);
        return;
    }

    printk("Acelerômetro configurado. Thread lendo a cada 1000 ms.\n");
    k_sem_give(&accel_ready_sem);
}
