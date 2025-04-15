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

//------------bluetooth---------------

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>


// Global buffer to store the current message
static char gatt_string_msg[100] = "Hello, World!";

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HTS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_DIS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

#define BT_UUID_GATT_STRING BT_UUID_DECLARE_16(BT_UUID_GATT_STRING_VAL)

static ssize_t read_gatt_string(struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, gatt_string_msg, strlen(gatt_string_msg));
}

static ssize_t write_gatt_string(struct bt_conn *conn, 
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, 
                                 uint16_t len,
                                 uint16_t offset, 
                                 uint8_t flags)
{
    char value[100]; // Adjust size as needed
    memcpy(value, buf, len);
    value[len] = '\0'; // Ensure null termination
    printk("Received string: %s\n", value);
    return len;
}
static void notify_subscribe_cb(const struct bt_gatt_attr *attr, uint16_t value)
{
    if (value == BT_GATT_CCC_NOTIFY) {
        printk("Client subscribed to notifications\n");
    } else {
        printk("Client unsubscribed from notifications\n");
    }
}

BT_GATT_SERVICE_DEFINE(gatt_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_GATT),
    BT_GATT_CHARACTERISTIC(BT_UUID_GATT_STRING,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, 
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           read_gatt_string, write_gatt_string, NULL),
	BT_GATT_CCC(notify_subscribe_cb, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);


static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

// static void bas_notify(void)
// {
// 	uint8_t battery_level = bt_bas_get_battery_level();

// 	battery_level--;

// 	if (!battery_level) {
// 		battery_level = 100U;
// 	}

// 	bt_bas_set_battery_level(battery_level);
// }


static void send_gatt_string(void)
{
    const char *msg = "Hello, World This is Lunar Vitals!";
    bt_gatt_notify(NULL, &gatt_service.attrs[1], msg, strlen(msg));
    printk("Sent: %s\n", msg);
}



//------------bluetooth---------------

#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);


#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2



//------------bluetooth---------------
//-------------LEDS--------------
#define LED1_NODE DT_ALIAS(led_custom_1)
#define LED2_NODE DT_ALIAS(led_custom_2)
static const struct device *gpio_dev;
#define BLUE_LED_PIN 24
#define RED_LED_PIN 2


void bluetooth_pairing_led(int status){
	gpio_pin_set(gpio_dev, BLUE_LED_PIN, status);
}

void error_led(int status){
	gpio_pin_set(gpio_dev, RED_LED_PIN, status);
}

bool collect_data = true;


static K_SEM_DEFINE(ble_init_ok, 0, 1);

static const struct i2c_dt_spec dev_max30102 = MAX30102_DT_SPEC;


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

void send_message_to_bluetooth(const char *msg)
{
	// Safely copy the message to the global buffer
	strncpy(gatt_string_msg, msg, sizeof(gatt_string_msg) - 1);
	gatt_string_msg[sizeof(gatt_string_msg) - 1] = '\0'; // ensure null-termination

	// Notify the client (if notifications are supported and enabled)
	bt_gatt_notify(NULL, &gatt_service.attrs[1], gatt_string_msg, strlen(gatt_string_msg));

	printk("Sent: %s\n", gatt_string_msg);
}

void configure_leds(void)
{
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0)); 

    if (!device_is_ready(gpio_dev)) {
        printk("Error: GPIO device is not ready.\n");
        return;
    }

    gpio_pin_configure(gpio_dev, BLUE_LED_PIN, GPIO_OUTPUT);
    gpio_pin_configure(gpio_dev, RED_LED_PIN, GPIO_OUTPUT);

    bluetooth_pairing_led(0);
    error_led(0);
}



int main(void)
{
    int blink_status = 0;
    int err = 0;

    configure_gpio();
	
    printk("Starting Lunar Vitals Program\n");
    configure_leds();
		
	//------------bluetooth---------------
	err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    bt_ready();
    bt_conn_auth_cb_register(&auth_cb_display);
	printk("UUID (16-bit): 0x%04X\n", BT_UUID_GATT_STRING_VAL);
	//------------bluetooth---------------
	//----------------------
	    /* Verify ADC readiness */
    adc_init();
	i2c_init();
	max30102_default_setup(&dev_max30102);
	//-------------------------
    // Main loop to blink LED to indicate status
    for (;;) {
		//printk(".\n");
		send_gatt_string();
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		if(collect_data){
			get_adc_data();
			i2c_read_data();
			for (int i = 0; i < 100; i++) {
				max30102_read_data_hr(&dev_max30102);
			}
		 }
		
        k_sleep(K_MSEC(1000));
    }

}