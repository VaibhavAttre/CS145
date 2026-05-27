/*
 * adc_driver.c
 *
 *  Created on: May 25, 2026
 *      Author: vaibh
 */

#include "adc_driver.h"
#include <stdint.h>

#define RCC_AHB2ENR *((volatile uint32_t *)(0x44020C8CUL))
#define GPIOA_MODER *((volatile uint32_t *)(0x42020000UL))
#define GPIOA_PUPDR *((volatile uint32_t *)(0x4202000CUL))

#define ADC1_BASE 0x42028000UL

#define ADC1_ISR (*((volatile uint32_t *)(ADC1_BASE)))
#define ADC1_CR  (*((volatile uint32_t *)(ADC1_BASE + 0x08)))
#define ADC1_CFGR  (*((volatile uint32_t *)(ADC1_BASE + 0x0C))) //not really needed
#define ADC1_SMPR2 (*((volatile uint32_t *)(ADC1_BASE + 0x18)))
#define ADC1_SQR1 (*((volatile uint32_t *)(ADC1_BASE + 0x30)))
#define ADC1_DR (*((volatile uint32_t *)(ADC1_BASE + 0x40)))

#define ADC_ISR_ADRDY    (1U << 0)
#define ADC_ISR_EOC      (1U << 2)
#define ADC_ISR_LDORDY   (1U << 12)

#define ADC_CR_ADEN      (1U << 0)
#define ADC_CR_ADSTART   (1U << 2)
#define ADC_CR_ADVREGEN  (1U << 28)
#define ADC_CR_DEEPPWD   (1U << 29)
#define ADC_CR_ADCALDIF  (1U << 16)
#define ADC_CR_ADCAL     (1U << 31)
