/**
 * @file startup_mspm0l222x_ticlang.c
 * @author Sumedh Girish
 * @brief Startup and Vector Table for TI Clang.
 *
 * Defines the interrupt vector table and basic reset handling for the
 * MSPM0L2228. This file is critical for ensuring the processor correctly
 * identifies the initial stack pointer and the reset entry point.
 */

#include "dl_config.h"
#include "ti/driverlib/dl_gpio.h"
#include <stdint.h>
#include <ti/devices/msp/msp.h>

/** @brief Top of the stack, defined in the linker command file. */
extern unsigned long __STACK_END;

/** @brief TI Clang C runtime initialization entry point. */
extern __NO_RETURN void __PROGRAM_START(void);

/* Forward declaration of the default fault handlers. */
void Default_Handler(void) __attribute__((weak));
extern void Reset_Handler(void) __attribute__((weak));

/* Processor Exceptions */
extern void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern void HardFault_Handler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
extern void PendSV_Handler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void SysTick_Handler(void)
    __attribute__((weak, alias("Default_Handler")));

/* Device Specific Interrupt Handlers */
extern void GROUP0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void GROUP1_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void TIMG12_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void UART4_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void ADC0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void SPI0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void SPI1_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void UART2_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void UART3_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void UART0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void UART1_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void TIMA0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void TIMG8_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void TIMG0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void TIMG4_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void TIMG5_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void I2C0_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void I2C1_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void I2C2_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void AESADV_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void LCD_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void LFSS_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));
extern void DMA_IRQHandler(void)
    __attribute__((weak, alias("Default_Handler")));

/**
 * @brief Interrupt vector table for the MSPM0L2228.
 *
 * Placed in the `.intvecs` section by the linker to reside at address 0x6000.
 * Contains the initial SP and pointers to exception/interrupt handlers.
 */
#if defined(__ARM_ARCH) && (__ARM_ARCH != 0)
void (*const interruptVectors[])(void) __attribute((used))
__attribute__((section(".intvecs"))) =
#elif defined(__TI_ARM__)
#pragma RETAIN(interruptVectors)
#pragma DATA_SECTION(interruptVectors, ".intvecs")
void (*const interruptVectors[])(void) =
#else
#error "Compiler not supported"
#endif
    {
        (void (*)(void))((uint32_t) &__STACK_END),
        /* The initial stack pointer */
        Reset_Handler,     /* The reset handler         */
        NMI_Handler,       /* The NMI handler           */
        HardFault_Handler, /* The hard fault handler    */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        SVC_Handler,       /* SVCall handler            */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        PendSV_Handler,    /* The PendSV handler        */
        SysTick_Handler,   /* SysTick handler           */
        GROUP0_IRQHandler, /* GROUP0 interrupt handler  */
        GROUP1_IRQHandler, /* GROUP1 interrupt handler  */
        TIMG12_IRQHandler, /* TIMG12 interrupt handler  */
        UART4_IRQHandler,  /* UART4 interrupt handler   */
        ADC0_IRQHandler,   /* ADC0 interrupt handler    */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        SPI0_IRQHandler,   /* SPI0 interrupt handler    */
        SPI1_IRQHandler,   /* SPI1 interrupt handler    */
        0,                 /* Reserved                  */
        0,                 /* Reserved                  */
        UART2_IRQHandler,  /* UART2 interrupt handler   */
        UART3_IRQHandler,  /* UART3 interrupt handler   */
        UART0_IRQHandler,  /* UART0 interrupt handler   */
        UART1_IRQHandler,  /* UART1 interrupt handler   */
        0,                 /* Reserved                  */
        TIMA0_IRQHandler,  /* TIMA0 interrupt handler   */
        0,                 /* Reserved                  */
        TIMG8_IRQHandler,  /* TIMG8 interrupt handler   */
        TIMG0_IRQHandler,  /* TIMG0 interrupt handler   */
        TIMG4_IRQHandler,  /* TIMG4 interrupt handler   */
        TIMG5_IRQHandler,  /* TIMG5 interrupt handler   */
        I2C0_IRQHandler,   /* I2C0 interrupt handler    */
        I2C1_IRQHandler,   /* I2C1 interrupt handler    */
        I2C2_IRQHandler,   /* I2C2 interrupt handler    */
        0,                 /* Reserved                  */
        AESADV_IRQHandler, /* AESADV interrupt handler*/
        LCD_IRQHandler,    /* LCD interrupt handler     */
        LFSS_IRQHandler,   /* LFSS interrupt handler    */
        DMA_IRQHandler     /* DMA interrupt handler     */
    };

/**
 * @brief Processor Reset Handler.
 *
 * This is the code that gets called when the processor first starts execution
 * following a reset event. It jumps to the TI Clang C initialization routine
 * which sets up the stack and calls main().
 */
void Reset_Handler(void)
{
    /* Disable SWD immediately on boot to prevent debugger attachment */
    DL_SYSCTL_disableSWD();

    /* Jump to the ticlang C Initialization Routine. */
    __asm("    .global _c_int00\n"
          "    b       _c_int00");
}

/**
 * @brief Default interrupt handler.
 *
 * This is the code that gets called when the processor receives an unexpected
 * interrupt. It turns off the Status LED and enters an infinite loop for
 * debugger inspection.
 */
void Default_Handler(void)
{
    DL_GPIO_clearPins(LEDS_PORT, LEDS_STATUS_LED_PIN);
    /* Enter an infinite loop. */
    while (1)
    {
    }
}
