/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdio.h>
#include <string.h>

#include "gap_api.h"
#include "gatt_api.h"

#include "os_timer.h"
#include "os_mem.h"
#include "sys_utils.h"
#include "button.h"
#include "jump_table.h"

#include "user_task.h"
#include "usart_device.h" // Added for UART test

#include "driver_plf.h"
#include "driver_system.h"
#include "driver_i2s.h"
#include "driver_pmu.h"
#include "driver_uart.h"
#include "driver_rtc.h"
#include "driver_flash.h"
#include "driver_efuse.h"
#include "flash_usage_config.h"
#include "flash_op.h"

#include "ble_simple_peripheral.h"
#include "simple_gatt_service.h"
#include "base_ble.h"

/* 声明 usart_device.c 中定义的全局变量 */
extern UART_Comm_Base_t g_uart1_dev;

/*
 * LOCAL VARIABLES
 */

const struct jump_table_version_t _jump_table_version __attribute__((section("jump_table_3"))) =
{
    .firmware_version = 0x00000000,
};

const struct jump_table_image_t _jump_table_image __attribute__((section("jump_table_1"))) =
{
    .image_type = IMAGE_TYPE_APP,
    .image_size = 0x19000,      
};


__attribute__((section("ram_code"))) void pmu_gpio_isr_ram(void)
{
    uint32_t gpio_value = ool_read32(PMU_REG_GPIOA_V);
    
    button_toggle_detected(gpio_value);
    ool_write32(PMU_REG_PORTA_LAST, gpio_value);
}

/*********************************************************************
 * @fn      user_custom_parameters
 *
 * @brief   initialize several parameters, this function will be called 
 *          at the beginning of the program. 
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
void user_custom_parameters(void)
{
    struct chip_unique_id_t id_data;

    efuse_get_chip_unique_id(&id_data);
    __jump_table.addr.addr[0] = 0xBD;
    __jump_table.addr.addr[1] = 0xAD;
    __jump_table.addr.addr[2] = 0xD0;
    __jump_table.addr.addr[3] = 0xF0;
    __jump_table.addr.addr[4] = 0x17;
    __jump_table.addr.addr[5] = 0x20;
    
    id_data.unique_id[5] |= 0xc0; // random addr->static addr type:the top two bit must be 1.
    memcpy(__jump_table.addr.addr,id_data.unique_id,6);
    __jump_table.system_clk = SYSTEM_SYS_CLK_48M;
    jump_table_set_static_keys_store_offset(JUMP_TABLE_STATIC_KEY_OFFSET);
    ble_set_addr_type(BLE_ADDR_TYPE_PUBLIC);
    retry_handshake();
}

/*********************************************************************
 * @fn      user_entry_before_sleep_imp
 *
 * @brief   Before system goes to sleep mode, user_entry_before_sleep_imp()
 *          will be called, MCU peripherals can be configured properly before 
 *          system goes to sleep, for example, some MCU peripherals need to be
 *          used during the system is in sleep mode. 
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
__attribute__((section("ram_code"))) void user_entry_before_sleep_imp(void)
{
	uart_putc_noint_no_wait(UART1, 's');
}

/*********************************************************************
 * @fn      user_entry_after_sleep_imp
 *
 * @brief   After system wakes up from sleep mode, user_entry_after_sleep_imp()
 *          will be called, MCU peripherals need to be initialized again, 
 *          this can be done in user_entry_after_sleep_imp(). MCU peripherals
 *          status will not be kept during the sleep. 
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
__attribute__((section("ram_code"))) void user_entry_after_sleep_imp(void)
{
        /* 恢复 UART1 IOMUX，保证 co_printf 醒来后还能走 PA2/PA3 */
        system_set_port_pull(GPIO_PA2, true);
        system_set_port_mux(GPIO_PORT_A, GPIO_BIT_2, PORTA2_FUNC_UART1_RXD);
        system_set_port_mux(GPIO_PORT_A, GPIO_BIT_3, PORTA3_FUNC_UART1_TXD);
    
        uart_init(UART1, BAUD_RATE_115200);
        //NVIC_EnableIRQ(UART1_IRQn);
		uart_putc_noint_no_wait(UART1, 'w');
        // Do some things here, can be uart print

        NVIC_EnableIRQ(PMU_IRQn);
}

/*********************************************************************
 * @fn      user_entry_before_ble_init
 *
 * @brief   Code to be executed before BLE stack to be initialized.
 *          Power mode configurations, PMU part driver interrupt enable, MCU 
 *          peripherals init, etc. 
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
void user_entry_before_ble_init(void)
{    
    /* set system power supply in BUCK mode */
    pmu_set_sys_power_mode(PMU_SYS_POW_BUCK);
#ifdef FLASH_PROTECT
    flash_protect_enable(1);
#endif
    pmu_enable_irq(PMU_ISR_BIT_ACOK
                   | PMU_ISR_BIT_ACOFF
                   | PMU_ISR_BIT_ONKEY_PO
                   | PMU_ISR_BIT_OTP
                   | PMU_ISR_BIT_LVD
                   | PMU_ISR_BIT_BAT
                   | PMU_ISR_BIT_ONKEY_HIGH);
    NVIC_EnableIRQ(PMU_IRQn);
    
    /* 提前打开 UART1 IOMUX，co_printf 走 PA2/PA3（与业务口 PA0/PA1 不冲突） */
    system_set_port_pull(GPIO_PA2, true);
    system_set_port_mux(GPIO_PORT_A, GPIO_BIT_2, PORTA2_FUNC_UART1_RXD);
    system_set_port_mux(GPIO_PORT_A, GPIO_BIT_3, PORTA3_FUNC_UART1_TXD);
    uart_init(UART1, BAUD_RATE_115200); // Initialize UART1 to prevent co_printf crash (even if pins are not connected)
}



// --- UART RX Test ---
static uint8_t rx_byte;
static uint8_t rx_buffer[128];
static uint16_t rx_idx = 0;

// Debug Timer to check UART Hardware Status
os_timer_t debug_timer;
static void debug_timer_func(void *arg)
{
    // Read Line Status Register (LSR) of UART0
    volatile struct uart_reg_t *uart_reg = (volatile struct uart_reg_t *)UART0;
    uint32_t lsr = uart_reg->lsr;
    
    // Check for errors
    if (lsr & 0x0E) { // Overrun, Parity, or Framing Error
        co_printf("UART0 Error: LSR=0x%02X\r\n", lsr);
    }
    
    os_timer_start(&debug_timer, 2000, 0);
}

static void uart_rx_callback(void *dummy, uint8_t status)
{
    // 1. Echo immediately (Critical for verification)
    if (g_uart1_dev.send) {
        g_uart1_dev.send(&g_uart1_dev, &rx_byte, 1);
    }

    // 2. Store in buffer
    if (rx_idx < sizeof(rx_buffer) - 1) {
        rx_buffer[rx_idx++] = rx_byte;
    }
    
    // 3. Check for line end
    if (rx_byte == '\n' || rx_idx >= sizeof(rx_buffer) - 1) {
        rx_buffer[rx_idx] = '\0';
        // Print to debug port
        co_printf("RX Line: %s", rx_buffer);
        rx_idx = 0;
    }
    
    // 4. Re-arm Interrupt for next byte
    uart1_read(&rx_byte, 1, uart_rx_callback);
}
// --------------------

/*********************************************************************
 * @fn      user_entry_after_ble_init
 *
 * @brief   Main entrancy of user application. This function is called after BLE stack
 *          is initialized, and all the application code will be executed from here.
 *          In that case, application layer initializtion can be startd here. 
 *
 * @param   None. 
 *       
 *
 * @return  None.
 */
void user_entry_after_ble_init(void)
{
    co_printf("BLE Peripheral\r\n");

    // --- Flash Test Start ---
    co_printf("Testing Flash Operation...\r\n");
    tpms_bind_info_t test_info_write = {
        .sensor_id = {0x11, 0x22, 0x33, 0x44},
        .status = 0xAA
    };
    tpms_bind_info_t test_info_read = {0};

    // 1. Write
    co_printf("Writing to addr 0x%x: ID=%02X%02X%02X%02X, Status=%02X\r\n", 
              TPMS_BINDING_INFO_SAVE_ADDR,
              test_info_write.sensor_id[0], test_info_write.sensor_id[1],
              test_info_write.sensor_id[2], test_info_write.sensor_id[3],
              test_info_write.status);
    flash_op_save_tpms_info(TPMS_BINDING_INFO_SAVE_ADDR, &test_info_write);

    // 2. Read
    flash_op_load_tpms_info(TPMS_BINDING_INFO_SAVE_ADDR, &test_info_read);
    co_printf("Read back: ID=%02X%02X%02X%02X, Status=%02X\r\n", 
              test_info_read.sensor_id[0], test_info_read.sensor_id[1],
              test_info_read.sensor_id[2], test_info_read.sensor_id[3],
              test_info_read.status);

    // 3. Verify
    if (memcmp(&test_info_write, &test_info_read, sizeof(tpms_bind_info_t)) == 0) {
        co_printf("Flash Test PASSED!\r\n");
    } else {
        co_printf("Flash Test FAILED!\r\n");
    }
    // --- Flash Test End ---

	
#if 1
    system_sleep_disable();		//disable sleep 
#else
    if(__jump_table.system_option & SYSTEM_OPTION_SLEEP_ENABLE)  //if sleep is enalbed, delay 3s for JLINK 
    {
        co_printf("\r\na");
        co_delay_100us(10000);       
        co_printf("\r\nb");
        co_delay_100us(10000);
        co_printf("\r\nc");
        co_delay_100us(10000);
        co_printf("\r\nd");
    }
#endif
		
    // User task initialization, for buttons.
    user_task_init();
    /* 将业务串口切换到 UART0 (PA0/PA1) 避免与 CH340 冲突 */
    UART_Device_Create(&g_uart1_dev, UART0, 115200);
    
    // --- 使用 SDK 中断式 uart1_read ---
    UART_Task_Init();

    // 发送一个启动信息，证明 TX 是好的
    char *hello = "UART1 Interrupt Ready (uart1_read).\r\n";
    g_uart1_dev.send(&g_uart1_dev, (uint8_t *)hello, strlen(hello));

    // Start Debug Timer
    os_timer_init(&debug_timer, debug_timer_func, NULL);
    os_timer_start(&debug_timer, 2000, 0);

    // Application layer initialization, can included bond manager init, 
    // advertising parameters init, scanning parameter init, GATT service adding, etc.    
    simple_peripheral_init();
	
}
