/**
 * @file    startup_stm32f103xb.s
 * @brief   STM32F103xB GCC startup file (Medium-density devices: C8, CB)
 *          ARM Cortex-M3 / GNU assembler format
 *
 * Memory:
 *   Flash  0x08000000  64 KB (C8T6)
 *   RAM    0x20000000  20 KB
 *
 * Generated compatible with STM32F1xx HAL (USE_HAL_DRIVER, STM32F103xB).
 * Use with linker script:  GCC/STM32F103C8TX.ld
 */

  .syntax unified
  .cpu cortex-m3
  .fpu softvfp
  .thumb

/* Stack size (bytes) – override via linker --defsym=_stack_size=N */
  .set Stack_Size, 0x400
  .section .stack, "aw", %nobits
  .align 3
Stack_Mem:
  .space Stack_Size
__initial_sp:

/* Heap size (bytes) */
  .set Heap_Size, 0x200
  .section .heap, "aw", %nobits
  .align 3
__heap_base:
Heap_Mem:
  .space Heap_Size
__heap_limit:

/* ---------- Vector Table -------------------------------------------------- */
  .section .isr_vector, "a", %progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word __initial_sp
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* External Interrupts */
  .word WWDG_IRQHandler
  .word PVD_IRQHandler
  .word TAMPER_IRQHandler
  .word RTC_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Channel1_IRQHandler
  .word DMA1_Channel2_IRQHandler
  .word DMA1_Channel3_IRQHandler
  .word DMA1_Channel4_IRQHandler
  .word DMA1_Channel5_IRQHandler
  .word DMA1_Channel6_IRQHandler
  .word DMA1_Channel7_IRQHandler
  .word ADC1_2_IRQHandler
  .word USB_HP_CAN1_TX_IRQHandler
  .word USB_LP_CAN1_RX0_IRQHandler
  .word CAN1_RX1_IRQHandler
  .word CAN1_SCE_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_IRQHandler
  .word TIM1_UP_IRQHandler
  .word TIM1_TRG_COM_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTC_Alarm_IRQHandler
  .word USBWakeUp_IRQHandler

/* ---------- Reset Handler ------------------------------------------------- */
  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  /* Copy .data section from Flash to RAM */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4

LoopCopyDataInit:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   CopyDataInit

  /* Zero fill .bss section */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     LoopFillZerobss

FillZerobss:
  str   r3, [r2]
  adds  r2, r2, #4

LoopFillZerobss:
  cmp   r2, r4
  bcc   FillZerobss

  /* Call the clock system initialization function */
  bl    SystemInit
  /* Call static constructors */
  bl    __libc_init_array
  /* Call the application's entry point */
  bl    main
  bx    lr

  .size Reset_Handler, .-Reset_Handler

/* ---------- Default Handlers (weak, redirected to infinite loop) ---------- */
  .section .text.Default_Handler, "ax", %progbits
Default_Handler:
Infinite_Loop:
  b     Infinite_Loop
  .size Default_Handler, .-Default_Handler

  .macro DefHandler sym
  .weak \sym
  .thumb_set \sym, Default_Handler
  .endm

  DefHandler NMI_Handler
  DefHandler HardFault_Handler
  DefHandler MemManage_Handler
  DefHandler BusFault_Handler
  DefHandler UsageFault_Handler
  DefHandler SVC_Handler
  DefHandler DebugMon_Handler
  DefHandler PendSV_Handler
  DefHandler SysTick_Handler
  DefHandler WWDG_IRQHandler
  DefHandler PVD_IRQHandler
  DefHandler TAMPER_IRQHandler
  DefHandler RTC_IRQHandler
  DefHandler FLASH_IRQHandler
  DefHandler RCC_IRQHandler
  DefHandler EXTI0_IRQHandler
  DefHandler EXTI1_IRQHandler
  DefHandler EXTI2_IRQHandler
  DefHandler EXTI3_IRQHandler
  DefHandler EXTI4_IRQHandler
  DefHandler DMA1_Channel1_IRQHandler
  DefHandler DMA1_Channel2_IRQHandler
  DefHandler DMA1_Channel3_IRQHandler
  DefHandler DMA1_Channel4_IRQHandler
  DefHandler DMA1_Channel5_IRQHandler
  DefHandler DMA1_Channel6_IRQHandler
  DefHandler DMA1_Channel7_IRQHandler
  DefHandler ADC1_2_IRQHandler
  DefHandler USB_HP_CAN1_TX_IRQHandler
  DefHandler USB_LP_CAN1_RX0_IRQHandler
  DefHandler CAN1_RX1_IRQHandler
  DefHandler CAN1_SCE_IRQHandler
  DefHandler EXTI9_5_IRQHandler
  DefHandler TIM1_BRK_IRQHandler
  DefHandler TIM1_UP_IRQHandler
  DefHandler TIM1_TRG_COM_IRQHandler
  DefHandler TIM1_CC_IRQHandler
  DefHandler TIM2_IRQHandler
  DefHandler TIM3_IRQHandler
  DefHandler TIM4_IRQHandler
  DefHandler I2C1_EV_IRQHandler
  DefHandler I2C1_ER_IRQHandler
  DefHandler I2C2_EV_IRQHandler
  DefHandler I2C2_ER_IRQHandler
  DefHandler SPI1_IRQHandler
  DefHandler SPI2_IRQHandler
  DefHandler USART1_IRQHandler
  DefHandler USART2_IRQHandler
  DefHandler USART3_IRQHandler
  DefHandler EXTI15_10_IRQHandler
  DefHandler RTC_Alarm_IRQHandler
  DefHandler USBWakeUp_IRQHandler
