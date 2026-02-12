#include "hx711.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static gpio_num_t pin_dout;
static gpio_num_t pin_sck;

void hx711_init(gpio_num_t dout, gpio_num_t sck)
{
    pin_dout = dout;
    pin_sck = sck;

    gpio_set_direction(pin_sck, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin_dout, GPIO_MODE_INPUT);

    gpio_set_level(pin_sck, 0);
}

int32_t hx711_read(void)
{
    int32_t count = 0;

    while (gpio_get_level(pin_dout)); // Esperar listo

    for (int i = 0; i < 24; i++) {
        gpio_set_level(pin_sck, 1);
        count = count << 1;
        gpio_set_level(pin_sck, 0);

        if (gpio_get_level(pin_dout)) {
            count++;
        }
    }

    // Pulso extra para ganancia 128
    gpio_set_level(pin_sck, 1);
    gpio_set_level(pin_sck, 0);

    // Extensión de signo
    if (count & 0x800000) {
        count |= ~0xffffff;
    }

    return count;
}
