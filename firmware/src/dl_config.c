/**
 * @file dl_config.c
 * @author Sumedh Girish
 * @brief Device Library Configuration Source File.
 *
 * Implements the hardware abstraction layer (HAL) initialization for the
 * MSPM0L2228. This includes clock tree setup, power domain enabling,
 * and peripheral configuration for UART, GPIO, and the TRNG.
 */

#include "dl_config.h"
#include "ti/devices/msp/m0p/mspm0l222x.h"
#include "ti/driverlib/dl_gpio.h"
#include "ti/driverlib/dl_trng.h"

/**
 * @brief Main Entrypoint for DriverLib Configuration.
 *
 * This function orchestrates the initialization sequence for all required
 * hardware modules. It must be called before any application logic.
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();

    // Module-Specific Initializations
    SYSCFG_DL_TRNG_init();
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_UART_0_init();
    SYSCFG_DL_UART_1_init();
}

/**
 * @brief Enable power lines and perform resets for hardware ports.
 *
 * Resets the state of GPIO and UART peripherals and then enables their
 * power domains. Includes a mandatory delay for stabilization.
 */
SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);

    DL_UART_reset(UART_0_INST);
    DL_UART_reset(UART_1_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);

    DL_UART_enablePower(UART_0_INST);
    DL_UART_enablePower(UART_1_INST);

    DL_TRNG_reset(TRNG);
    DL_TRNG_enablePower(TRNG);

    delay_cycles(POWER_STARTUP_DELAY);
}

/**
 * @brief Initialize GPIO peripherals and IOMUX.
 *
 * Configures the secondary functions for UART pins and initializes the
 * Status LED pin as a digital output.
 */
SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{
    DL_GPIO_initPeripheralOutputFunction(GPIO_UART_0_IOMUX_TX,
                                         GPIO_UART_0_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_UART_0_IOMUX_RX,
                                        GPIO_UART_0_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(GPIO_UART_1_IOMUX_TX,
                                         GPIO_UART_1_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_UART_1_IOMUX_RX,
                                        GPIO_UART_1_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(LEDS_STATUS_LED_IOMUX);

    DL_GPIO_setPins(LEDS_PORT, LEDS_STATUS_LED_PIN);
    DL_GPIO_enableOutput(LEDS_PORT, LEDS_STATUS_LED_PIN);
}

/**
 * @brief Initialize System Control (SYSCTL).
 *
 * Configures the system for maximum brownout protection by setting the
 * BOR threshold to Level 3 (~2.7V) and activating the monitoring hardware.
 * Also configures the system oscillator frequency to its base (32 MHz).
 */
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{
    // Configure for maximum strictness (BOR3 ~2.7V)
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_3);
    DL_SYSCTL_activateBORThreshold();

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIVIDER_DISABLE);

    // Partition SRAM: lower 28KB = RW (data/stack), upper 4KB = RX (ramfuncs)
    // Address sourced from linker symbol __sram_boundary = ORIGIN(SRAM_CODE)
    extern uint32_t __sram_boundary;
    DL_SYSCTL_setSRAMBoundaryAddress((uint32_t) &__sram_boundary);
}

/** @brief Clock configuration for UART 0. */
static const DL_UART_Main_ClockConfig gUART_0ClockConfig = {
    .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1};

/** @brief Protocol configuration for UART 0 (8N1). */
static const DL_UART_Main_Config gUART_0Config = {
    .mode = DL_UART_MAIN_MODE_NORMAL,
    .direction = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity = DL_UART_MAIN_PARITY_NONE,
    .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits = DL_UART_MAIN_STOP_BITS_ONE};

/**
 * @brief Initialize UART 0 (The Host Interface).
 *
 * Configures the baud rate to 115200 based on the 32 MHz BUSCLK.
 */
SYSCONFIG_WEAK void SYSCFG_DL_UART_0_init(void)
{
    DL_UART_Main_setClockConfig(
        UART_0_INST, (DL_UART_Main_ClockConfig *) &gUART_0ClockConfig);

    DL_UART_Main_init(UART_0_INST, (DL_UART_Main_Config *) &gUART_0Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART_0_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_0_INST, UART_0_IBRD_32_MHZ_115200_BAUD,
                                    UART_0_FBRD_32_MHZ_115200_BAUD);

    DL_UART_Main_enable(UART_0_INST);
}

/** @brief Clock configuration for UART 1. */
static const DL_UART_Main_ClockConfig gUART_1ClockConfig = {
    .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1};

/** @brief Protocol configuration for UART 1 (8N1). */
static const DL_UART_Main_Config gUART_1Config = {
    .mode = DL_UART_MAIN_MODE_NORMAL,
    .direction = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity = DL_UART_MAIN_PARITY_NONE,
    .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits = DL_UART_MAIN_STOP_BITS_ONE};

/**
 * @brief Initialize UART 1 (The Engineering/Peer Interface).
 *
 * Configures the baud rate to 115200 based on the 32 MHz BUSCLK.
 */
SYSCONFIG_WEAK void SYSCFG_DL_UART_1_init(void)
{
    DL_UART_Main_setClockConfig(
        UART_1_INST, (DL_UART_Main_ClockConfig *) &gUART_1ClockConfig);

    DL_UART_Main_init(UART_1_INST, (DL_UART_Main_Config *) &gUART_1Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART_1_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_1_INST, UART_1_IBRD_32_MHZ_115200_BAUD,
                                    UART_1_FBRD_32_MHZ_115200_BAUD);

    DL_UART_Main_enable(UART_1_INST);
}

/**
 * @brief Performs a secure warm-up and health check of the TRNG hardware.
 *
 * 1. Discards the first 128 words to flush initial oscillator bias.
 * 2. Verifies that the hardware health tests (repetition/adaptive) have passed.
 * 3. Halts the system via Default_Handler if a failure is detected.
 */
static void TRNG_SecureWarmup(void)
{
    for (int i = 0; i < 128; i++)
    {
        while (!DL_TRNG_isCaptureReady(TRNG))
            ;
        (void) DL_TRNG_getCapture(TRNG);
    }

    if (DL_TRNG_isHealthTestFail(TRNG))
    {
        Default_Handler();
    }
}

/**
 * @brief Initialize True Random Number Generator (TRNG).
 */
SYSCONFIG_WEAK void SYSCFG_DL_TRNG_init(void)
{
    DL_TRNG_setDecimationRate(TRNG, DL_TRNG_DECIMATION_RATE_8);
    DL_TRNG_sendCommand(TRNG, DL_TRNG_CMD_NORM_FUNC);

    TRNG_SecureWarmup();
}
