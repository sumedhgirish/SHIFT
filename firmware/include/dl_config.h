/**
 * @file dl_config.h
 * @author Sumedh Girish
 * @brief Device Library Configuration for MSPM0.
 *
 * This header contains the hardware abstraction layer configuration for the
 * SHIFT firmware. It defines the pinout, clock frequencies, and peripheral
 * instances for the TI MSPM0L2228 microcontroller.
 */

#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0L222X
#define CONFIG_MSPM0L2228

#define SYSCONFIG_WEAK __attribute__((weak))

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

/**
 * @brief Performs all required MSP DriverLib initialization.
 *
 * Orchestrates the full hardware startup sequence: power domains, system
 * clocks, GPIO muxing, TRNG seed initialization, and dual-channel UART
 * configuration. This function must be called as the first action in main().
 */
void SYSCFG_DL_init(void);

/* clang-format off */

/** @brief Delay cycles required for power domain stabilization during startup. */
#define POWER_STARTUP_DELAY                                                (16)

/** @brief Central CPU clock frequency (32MHz). */
#define CPUCLK_FREQ                                                     32000000

/* Port definition for Pin Group LEDS */
#define LEDS_PORT                                                        (GPIOB)

/* Defines for STATUS_LED: GPIOB.14 with pinCMx 35 on package pin 2 */
#define LEDS_STATUS_LED_PIN                                     (DL_GPIO_PIN_14)
#define LEDS_STATUS_LED_IOMUX                                    (IOMUX_PINCM35)

/* Defines for UART_0 (Host Interface) */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           32000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                        DL_GPIO_PIN_11
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_10
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM26)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM25)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM26_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM25_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_32_MHZ_115200_BAUD                                      (17)
#define UART_0_FBRD_32_MHZ_115200_BAUD                                      (23)

/* Defines for UART_1 (Peer/Engineering Interface) */
#define UART_1_INST                                                        UART1
#define UART_1_INST_FREQUENCY                                           32000000
#define UART_1_INST_IRQHandler                                  UART1_IRQHandler
#define UART_1_INST_INT_IRQN                                      UART1_INT_IRQn
#define GPIO_UART_1_RX_PORT                                                GPIOA
#define GPIO_UART_1_TX_PORT                                                GPIOA
#define GPIO_UART_1_RX_PIN                                         DL_GPIO_PIN_9
#define GPIO_UART_1_TX_PIN                                         DL_GPIO_PIN_8
#define GPIO_UART_1_IOMUX_RX                                     (IOMUX_PINCM20)
#define GPIO_UART_1_IOMUX_TX                                     (IOMUX_PINCM19)
#define GPIO_UART_1_IOMUX_RX_FUNC                      IOMUX_PINCM20_PF_UART1_RX
#define GPIO_UART_1_IOMUX_TX_FUNC                      IOMUX_PINCM19_PF_UART1_TX
#define UART_1_BAUD_RATE                                                (115200)
#define UART_1_IBRD_32_MHZ_115200_BAUD                                      (17)
#define UART_1_FBRD_32_MHZ_115200_BAUD                                      (23)

/* clang-format on */

/**
 * @brief Enables power and resets peripherals.
 *
 * Configures power domains for GPIOA, GPIOB, UART0, UART1, and TRNG.
 * Includes a mandatory startup delay for supply stabilization.
 */
void SYSCFG_DL_initPower(void);

/**
 * @brief Initializes GPIO muxing and pin directions.
 *
 * Assigns pins to their respective peripheral functions (UART RX/TX) and
 * configures output pins like the Status LED.
 */
void SYSCFG_DL_GPIO_init(void);

/**
 * @brief Configures System Control (SYSCTL) settings.
 *
 * Implements high-strictness brownout monitoring (BOR3) and system clock tree
 * configuration (32MHz base frequency).
 */
void SYSCFG_DL_SYSCTL_init(void);

/**
 * @brief Initializes UART0 for Host Communication (115200 baud).
 */
void SYSCFG_DL_UART_0_init(void);

/**
 * @brief Initializes UART1 for Inter-HSM Communication (115200 baud).
 */
void SYSCFG_DL_UART_1_init(void);

/**
 * @brief Initializes the hardware True Random Number Generator.
 */
void SYSCFG_DL_TRNG_init(void);

#endif /* ti_msp_dl_config_h */
