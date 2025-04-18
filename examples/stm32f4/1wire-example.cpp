// Example for STM32F4DISCOVERY development board

#include "FreeRTOS.h"
#include "task.h"
#include "periph/systick.hpp"
#include "periph/gpio_stm32f4.hpp"
#include "periph/dma_stm32f4.hpp"
#include "periph/uart_stm32f4.hpp"
#include "drivers/gpio_pin_debouncer.hpp"
#include "drivers/onewire.hpp"

struct task_params_t
{
    drv::gpio_pin_debouncer &button;
    drv::onewire &onewire;
    periph::gpio &led;
};

static void button_1_task(void *pvParameters)
{
    task_params_t *task_params = (task_params_t *)pvParameters;
    drv::gpio_pin_debouncer &button_1 = task_params->button;
    drv::onewire &ds18b20 = task_params->onewire;
    periph::gpio &green_led = task_params->led;
    
    while(1)
    {
        bool new_state;
        if(button_1.poll_1ms(new_state))
        {
            if(new_state)
            {
                green_led.toggle();
                
                uint64_t rom;
                auto res = ds18b20.read_rom(rom);
                if(res == drv::onewire::res::ok)
                {
                    uint8_t write_buff[3] = {0x01, 0x02, 0x03};
                    res = ds18b20.write(rom, write_buff, 3);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

int main(int argc, char *argv[])
{
    periph::systick::init();
    
    // Green LED
    periph::gpio_stm32f4 green_led(periph::gpio_stm32f4::port::d, 12, periph::gpio::mode::digital_output, 1);
    
    // Button 1
    periph::gpio_stm32f4 button_1_gpio(periph::gpio_stm32f4::port::a, 0, periph::gpio::mode::digital_input);
    drv::gpio_pin_debouncer button_1(button_1_gpio, std::chrono::milliseconds(50), 1);
    
    // 1Wire interface (connected to ds18b20)
    periph::gpio_stm32f4 uart_tx(periph::gpio_stm32f4::port::d, 8, periph::gpio::mode::alternate_function);
    periph::gpio_stm32f4 uart_rx(periph::gpio_stm32f4::port::b, 11, periph::gpio::mode::alternate_function);
    periph::dma_stm32f4 uart3_tx_dma(1, 3, 4, periph::dma_stm32f4::direction::memory_to_periph, 8);
    periph::dma_stm32f4 uart3_rx_dma(1, 1, 4, periph::dma_stm32f4::direction::periph_to_memory, 8);
    periph::uart_stm32f4 uart3(3, 115200, periph::uart_stm32f4::stopbits::stopbits_1,
        periph::uart_stm32f4::parity::none, uart3_tx_dma, uart3_rx_dma, uart_tx, uart_rx);
    drv::onewire onewire(uart3);
    
    task_params_t task_params = {button_1, onewire, green_led};
    xTaskCreate(button_1_task, "button_1", configMINIMAL_STACK_SIZE, &task_params, 1, nullptr);
    
    vTaskStartScheduler();
}
