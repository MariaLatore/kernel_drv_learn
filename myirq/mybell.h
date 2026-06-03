#ifndef IRQDOORBELL_REGS_H
#define IRQDOORBELL_REGS_H

#include <linux/bitops.h>
#include <linux/io.h>
#include <asm-generic/int-ll64.h>

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

/* STATUS bits */
#define MB_STATUS_EVENT_ACTIVE BIT(0)

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

#endif /* IRQDOORBELL_REGS_H */
