#ifndef IRQDOORBELL_REGS_H
#define IRQDOORBELL_REGS_H

#include <asm-generic/int-ll64.h>
#include <linux/bitops.h>
#include <linux/io.h>

struct mybell_dev {
  struct platform_device *pdev;
  void __iomem *base;
  int irq;
  u32 irq_count;
};

/* Register offsets */
#define MB_REG_STATUS 0x00
#define MB_REG_IRQ_ENABLE 0x04
#define MB_REG_IRQ_STATUS 0x08
#define MB_REG_IRQ_ACK 0x0c
#define MB_REG_EVENT_COUNT 0x10
#define MB_REG_THRESHOLD 0x14
#define MB_REG_SAMPLE_VALUE 0x18
#define MB_REG_ERROR_FLAGS 0x1c

/* STATUS bits */
#define MB_STATUS_EVENT_ACTIVE BIT(0)
#define MB_STATUS_OVER_THRESHOLD BIT(1)

/* IRQ_ENABLE bits */
#define MB_IRQ_ENABLE_EN BIT(0)

/* IRQ_STATUS bits */
#define MB_IRQ_STATUS_PENDING BIT(0)

static inline u32 mb_readl(struct mybell_dev *d, u32 reg) {
  return readl(d->base + reg);
}

static inline void mb_writel(struct mybell_dev *d, u32 reg, u32 val) {
  writel(val, d->base + reg);
}

static inline u32 mb_get_status(struct mybell_dev *d) {
  return mb_readl(d, MB_REG_STATUS);
}
static inline u32 mb_get_irq_status(struct mybell_dev *d) {
  return mb_readl(d, MB_REG_IRQ_STATUS);
}

static inline u32 mb_get_event_count(struct mybell_dev *d) {
  return mb_readl(d, MB_REG_EVENT_COUNT);
}

static inline void mb_enable_irq(struct mybell_dev *d) {
  mb_writel(d, MB_REG_IRQ_ENABLE, MB_IRQ_ENABLE_EN);
}

static inline void mb_disable_irq(struct mybell_dev *d) {
  mb_writel(d, MB_REG_IRQ_ENABLE, 0);
}

static inline void mb_ack_irq(struct mybell_dev *d) {
  mb_writel(d, MB_REG_IRQ_ACK, 1);
}

static inline u32 mb_get_threshold(struct mybell_dev *d) {
  return mb_readl(d, MB_REG_THRESHOLD);
}

static inline void mb_set_threshold(struct mybell_dev *d, u32 v) {
  mb_writel(d, MB_REG_THRESHOLD, v);
}

static inline u32 mb_get_sample_value(struct mybell_dev *d) {
  return mb_readl(d, MB_REG_SAMPLE_VALUE);
}

static inline u32 mb_get_error_flags(struct mybell_dev *d) {
  return mb_readl(d, MB_REG_ERROR_FLAGS);
}

#endif /* IRQDOORBELL_REGS_H */
