/* ---------- RCC ---------- */
#define RCC_AHB2ENR  (*(volatile uint32_t *)0x54020C8CUL)

/* ---------- GPIO bases ---------- */
#define GPIOB_BASE   0x52020400UL
#define GPIOE_BASE   0x52021000UL
#define GPIOF_BASE   0x52021400UL
#define GPIOG_BASE   0x52021800UL

/* ---------- GPIO registers ---------- */
#define GPIO_MODER(b)   (*(volatile uint32_t *)((b) + 0x00))
#define GPIO_OTYPER(b)  (*(volatile uint32_t *)((b) + 0x04))
#define GPIO_PUPDR(b)   (*(volatile uint32_t *)((b) + 0x0C))
#define GPIO_IDR(b)     (*(volatile uint32_t *)((b) + 0x10))
#define GPIO_BSRR(b)    (*(volatile uint32_t *)((b) + 0x18))

/* ---------- LED: PF4 = LD2 yellow ---------- */
#define LED_BASE  GPIOF_BASE
#define LED_PIN   4

typedef struct { uint32_t base; uint32_t pin; } Pin;

/*
 * ROWS = outputs (driven LOW one at a time, HIGH when idle)
 * Keypad ribbon pin 1-4 = Row 1-4
 */
static const Pin ROW[4] = {
    {GPIOB_BASE, 7},   /* Row 1 -> D0 -> PB7  */
    {GPIOB_BASE, 6},   /* Row 2 -> D1 -> PB6  */
    {GPIOG_BASE, 14},  /* Row 3 -> D2 -> PG14 */
    {GPIOE_BASE, 13},  /* Row 4 -> D3 -> PE13 */
};

/*
 * COLS = inputs with pull-up (read 0 when key pressed)
 * Keypad ribbon pin 5-8 = Col 1-4
 */
static const Pin COL[4] = {
    {GPIOE_BASE, 14},  /* Col 1 -> D4 -> PE14 */
    {GPIOE_BASE, 11},  /* Col 2 -> D5 -> PE11 */
    {GPIOE_BASE, 9},   /* Col 3 -> D6 -> PE9  */
    {GPIOG_BASE, 12},  /* Col 4 -> D7 -> PG12 */
};
