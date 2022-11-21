#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

#define MAX_CYCLES 65535
#define ON_SEC 30

const uint PIR_PIN = 10;
const uint LED_PIN = 11;  // LED that signals motion detection
const uint BUSY_PIN = 25; // Built-in LED of Pico  
const uint PHOTO_PIN = 26;
const uint POTENTIOMETER_PIN = 27;
const uint CALIBRATION_TIME = 6;
uint slice;
uint channel;
static bool movement;
static int transition = 1;

//
// IRQ handler called when rising or falling edge is detected
//
void pir_irq_handler(uint gpio, uint32_t event) {
    
    if (event == GPIO_IRQ_EDGE_RISE) // rising edge => detection of movement
    {
        movement = true;
        gpio_put(LED_PIN, 1);
    }
    else // falling edge
    {
        movement = false;
        gpio_put(LED_PIN, 0);
    }
}


//
// function used to calibrate PIR sensor
//
void calibrate () {
    for (uint counter = 0; counter < CALIBRATION_TIME; counter++){
      gpio_put(BUSY_PIN, 1);
      sleep_ms(500);
      gpio_put(BUSY_PIN, 0);
      sleep_ms(500);
    }    
    puts("Calibration completed");
}

int main()
{
    stdio_init_all();
    gpio_init(LED_PIN); // init LED Pin: used to signal motion detection
    gpio_set_dir(LED_PIN, GPIO_OUT); // LED Pin is an output pin
    gpio_init(BUSY_PIN); // init BUSY Pin: used to blink during calibration
    gpio_set_dir(BUSY_PIN, GPIO_OUT); // BUSY Pin is an output pin

    const float conversion_factor = 100.0f / (1 << 12);
    uint16_t light_lvl;
    uint16_t light_threshold;

    // Tell the LED pin that the PWM is in charge of its value.
    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    // Get the PWM slice from GPIO pin
    slice = pwm_gpio_to_slice_num (LED_PIN); 
    // Get the PWM channel from GPIO pin
    channel = pwm_gpio_to_channel (LED_PIN);

    uint16_t wrap = MAX_CYCLES;

    // turn it on
    pwm_set_enabled(slice, true);

    // Set the wrap point
    pwm_set_wrap(slice, wrap);
    // Set the set point
    // pwm_set_chan_level(slice, channel, wrap/2);

     // Calibrate PIR for CALIBRATION_TIME seconds
    calibrate();
   
    // Enable interrupt handling for PIR Pin:
    // Interrupt hanling for rising or falling edges
    gpio_set_irq_enabled_with_callback(PIR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pir_irq_handler);

    adc_init();
    adc_gpio_init(PHOTO_PIN);
    adc_gpio_init(POTENTIOMETER_PIN);

    while(true)
    {
        if (!movement) {
            // if the LED_PIN is turned on, dim the light down until it's off
            if (gpio_get(LED_PIN)){
                for (int i = 255; i >= 0; i--) 
                {
                    pwm_set_chan_level(slice, channel, i*i);
                    sleep_ms((transition*1000)/255);
                }
            }

            while (!movement) {
                adc_select_input(0);
                light_lvl = adc_read();
                adc_select_input(1);
                light_threshold = adc_read();
                printf("OFF Light Level: %f%%   Light Threshold: %f%%\n", light_lvl * conversion_factor, light_threshold * conversion_factor);
                sleep_ms(500);
            }


        } else if (light_lvl <= light_threshold) {
            // If the LED_PIN is off, gradually increase the voltage until the lights are fully on
            if (!gpio_get(LED_PIN)){
                for (int i = 0; i <= 255; i++) 
                {
                    pwm_set_chan_level(slice, channel, i*i);
                    sleep_ms((transition*1000)/255);
                }
            }

            // Checking every millisecond to see if there's movement. If so, then it will reset the counter
            // and by doing that keep the lights on. otherwise it will exit the for loop.
            for (uint counter = 0; counter < ON_SEC*1000; counter++) {
                if (movement) {
                    counter = 0;
                }
                sleep_ms(1);
            }

        } else {
            printf("Too much light compared to light threshold\n");
            sleep_ms(1000);
        }

    }

    return 0;
}
