#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "adc.h"
#include "MPU6050.h"
#include "MLX90614.h"
#include "BMP280.h"
#include "MAX30102.h"
#include "heart_rate.h"
#include "i2c.h"

#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2

#define KEY_PASSKEY_ACCEPT DK_BTN1_MSK
#define KEY_PASSKEY_REJECT DK_BTN2_MSK

// Define the interval at which to send the message (e.g., every 5 seconds)
#define MESSAGE_INTERVAL K_MSEC(5000)

//-------------LEDS--------------
#define LED1_NODE DT_ALIAS(led_custom_1)
#define LED2_NODE DT_ALIAS(led_custom_2)
#define LED3_NODE DT_ALIAS(led_custom_3)
static const struct device *gpio_dev;
#define GREEN_LED_PIN 4
#define BLUE_LED_PIN 29
#define RED_LED_PIN 28

//------------------------------
bool collect_data = true;
//-----------------------



static const struct i2c_dt_spec dev_max30102 = MAX30102_DT_SPEC;

void bluetooth_pairing_led(int status){
	gpio_pin_set(gpio_dev, BLUE_LED_PIN, status);
	
	printf("Blue led %d\n", status);
}

void error_led(int status){
	gpio_pin_set(gpio_dev, RED_LED_PIN, status);
	printf("Red led %d\n", status);
	
}

void connected_led(int status){
	gpio_pin_set(gpio_dev, GREEN_LED_PIN, status);
	printf("Green led %d\n", status);
}



void configure_leds(void)
{
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0)); // Assuming GPIO port 0

    if (!device_is_ready(gpio_dev)) {
        printk("Error: GPIO device is not ready.\n");
        return;
    }

    gpio_pin_configure(gpio_dev, GREEN_LED_PIN, GPIO_OUTPUT);
    gpio_pin_configure(gpio_dev, BLUE_LED_PIN, GPIO_OUTPUT);
    gpio_pin_configure(gpio_dev, RED_LED_PIN, GPIO_OUTPUT);

    bluetooth_pairing_led(0);
    error_led(0);
    connected_led(0);
}



void button_changed(uint32_t button_state, uint32_t has_changed)
{
   uint32_t buttons = button_state & has_changed;

    // Ignore debounce by checking if the new state is 0 (button quickly released)
    if (button_state == 0) {
        return;
    }


    printf("Button changed: state=0x%08X, changed=0x%08X, pressed=0x%08X\n",
           button_state, has_changed, buttons);

	if(buttons == 1){
		//start pairing 
		 // Start Bluetooth advertising
		// int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		// if (err) {
		// 	printk("error starting pairing\n");
			
		// 	return 0;
		// }
		// printk("started pairing\n");
		// 	LOG_INF("Advertising started");
		bluetooth_pairing_led(1);
	}

	if(buttons == 4){
		
		collect_data = !collect_data;
		error_led(!collect_data);
		if(collect_data){
			 printf("Collecting Data\n");
		}
		else{
			printf("Pausing Data Collection\n");
		}
	}
    
}

static void configure_gpio(void)
{
	int err;

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}
#endif /* CONFIG_BT_NUS_SECURITY_ENABLED */

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	} 
}


// // Function to send the message over Bluetooth
void send_message_to_bluetooth(const char *msg)
{
    printk("Starting send_message_to_bluetooth.\n");
}

//     // // Check if a Bluetooth connection is established
//     // if (!current_conn) {
//     //     printk(".");
//     //     return;
//     // }

//     int plen = strlen(msg);
//     int loc = 0;
//     // printk("Message length: %d\n", plen);
//     // printk("Message content: \"%s\"\n", msg);

//     // Copy the message into the nus_data buffer
//     while (plen > 0) {
//         //printk("Copying message to buffer. Remaining length: %d, Current location: %d\n", plen, loc);

//         // Copy the next chunk of the message to the buffer
//         int chunk_size = (plen > sizeof(nus_data.data) - nus_data.len) ? (sizeof(nus_data.data) - nus_data.len) : plen;
//         memcpy(&nus_data.data[nus_data.len], &msg[loc], chunk_size);
//         nus_data.len += chunk_size;
//         loc += chunk_size;
//         plen -= chunk_size;

//         // printk("Buffer length after copy: %d\n", nus_data.len);
//         // printk("Buffer content: \"%.*s\"\n", nus_data.len, nus_data.data);

//         // If the buffer is full or the message ends with a newline or carriage return, send it over BLE
//         if (nus_data.len >= sizeof(nus_data.data) || 
//             (nus_data.data[nus_data.len - 1] == '\n') || 
//             (nus_data.data[nus_data.len - 1] == '\r')) {
//             //printk("Buffer is ready to send. Sending data over BLE.\n");

//             // Send data over BLE connection
//             if (bt_nus_send(NULL, nus_data.data, nus_data.len)) {
//                 LOG_WRN("Failed to send data over BLE connection");
//                 printk("Warning: Failed to send data over BLE connection.\n");
//             } else {
//                 //printk("Data successfully sent over BLE. Data: \"%.*s\"\n", nus_data.len, nus_data.data);
                
// 				printk("%.*s", nus_data.len, nus_data.data);
//             }

//             // Reset buffer for the next message part
//             nus_data.len = 0;
//         }
//     }

//     //printk("send_message_to_bluetooth completed.\n");
// }




int main(void)
{
    int blink_status = 0;
    int err = 0;

    configure_gpio();
	
    printk("Starting Program\n");
	printk("Configuring LEDSm\n");
    configure_leds();
    
	printk("Advertising started");
	bluetooth_pairing_led(1);

	//----------------------
	    /* Verify ADC readiness */
    adc_init();
	i2c_init();
	max30102_default_setup(&dev_max30102);
	//-------------------------
    // Main loop to blink LED to indicate status
	int counter = 0;
    for (;;) {
		
		if(collect_data){
			if (counter == 0){
				get_adc_data();
				i2c_read_data();
			}
			max30102_read_data_hr(&dev_max30102);
		}
        dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		
        k_sleep(K_MSEC(10));
		counter += 1;
		counter = counter % 10;
    }

}


// void ble_write_thread(void)
// {
// 	/* Don't go any further until BLE is initialized */
// 	k_sem_take(&ble_init_ok, K_FOREVER);
// 	struct uart_data_t nus_data = {
// 		.len = 0,
// 	};

// 	for (;;) {
// 		/* Wait indefinitely for data to be sent over bluetooth */
// 		struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
// 						     K_FOREVER);

// 		int plen = MIN(sizeof(nus_data.data) - nus_data.len, buf->len);
// 		int loc = 0;

// 		while (plen > 0) {
// 			memcpy(&nus_data.data[nus_data.len], &buf->data[loc], plen);
// 			nus_data.len += plen;
// 			loc += plen;

// 			if (nus_data.len >= sizeof(nus_data.data) ||
// 			   (nus_data.data[nus_data.len - 1] == '\n') ||
// 			   (nus_data.data[nus_data.len - 1] == '\r')) {
// 				if (bt_nus_send(NULL, nus_data.data, nus_data.len)) {
// 					LOG_WRN("Failed to send data over BLE connection");
// 				}
// 				nus_data.len = 0;
// 			}

// 			plen = MIN(sizeof(nus_data.data), buf->len - loc);
// 		}

// 		k_free(buf);
// 	}
// }

// K_THREAD_DEFINE(ble_write_thread_id, STACKSIZE, ble_write_thread, NULL, NULL,
// 		NULL, PRIORITY, 0, 0);
