#include "armv7_p15.h"
#include "gic.h"
#include <timer.h>
#include <log/uart_print.h>
#include <asm-arm_inline.h>
#include <hvmm_trace.h>
#include <interrupt.h>

#include <config/cfg_platform.h>

enum generic_timer_type {
    GENERIC_TIMER_HYP,      /* IRQ 26 */
    GENERIC_TIMER_VIR,      /* IRQ 27 */
    GENERIC_TIMER_NSP,      /* IRQ 30 */
    GENERIC_TIMER_NUM_TYPES
};

enum {
    GENERIC_TIMER_REG_FREQ,
    GENERIC_TIMER_REG_HCTL,
    GENERIC_TIMER_REG_KCTL,
    GENERIC_TIMER_REG_HYP_CTRL,
    GENERIC_TIMER_REG_HYP_TVAL,
    GENERIC_TIMER_REG_HYP_CVAL,
    GENERIC_TIMER_REG_PHYS_CTRL,
    GENERIC_TIMER_REG_PHYS_TVAL,
    GENERIC_TIMER_REG_PHYS_CVAL,
    GENERIC_TIMER_REG_VIRT_CTRL,
    GENERIC_TIMER_REG_VIRT_TVAL,
    GENERIC_TIMER_REG_VIRT_CVAL,
    GENERIC_TIMER_REG_VIRT_OFF,
};

#define GENERIC_TIMER_CTRL_ENABLE       (1 << 0)
#define GENERIC_TIMER_CTRL_IMASK        (1 << 1)
#define GENERIC_TIMER_CTRL_ISTATUS      (1 << 2)
#define generic_timer_pcounter_read()   read_cntpct()
#define generic_timer_vcounter_read()   read_cntvct()

static uint32_t _timer_irqs[GENERIC_TIMER_NUM_TYPES];
static uint32_t _tvals[GENERIC_TIMER_NUM_TYPES];
static timer_callback_t _callback[GENERIC_TIMER_NUM_TYPES];
static enum generic_timer_type _timer_type = GENERIC_TIMER_HYP;

static inline void generic_timer_reg_write(int reg, uint32_t val)
{
    switch (reg) {
    case GENERIC_TIMER_REG_FREQ:
        write_cntfrq(val);
        break;
    case GENERIC_TIMER_REG_HCTL:
        write_cnthctl(val);
        break;
    case GENERIC_TIMER_REG_KCTL:
        write_cntkctl(val);
        break;
    case GENERIC_TIMER_REG_HYP_CTRL:
        write_cnthp_ctl(val);
        break;
    case GENERIC_TIMER_REG_HYP_TVAL:
        write_cnthp_tval(val);
        break;
    case GENERIC_TIMER_REG_PHYS_CTRL:
        write_cntp_ctl(val);
        break;
    case GENERIC_TIMER_REG_PHYS_TVAL:
        write_cntp_tval(val);
        break;
    case GENERIC_TIMER_REG_VIRT_CTRL:
        write_cntv_ctl(val);
        break;
    case GENERIC_TIMER_REG_VIRT_TVAL:
        write_cntv_tval(val);
        break;
    default:
        uart_print("Trying to write invalid generic-timer register\n\r");
        break;
    }
    isb();
}

static inline uint32_t generic_timer_reg_read(int reg)
{
    uint32_t val;
    switch (reg) {
    case GENERIC_TIMER_REG_FREQ:
        val = read_cntfrq();
        break;
    case GENERIC_TIMER_REG_HCTL:
        val = read_cnthctl();
        break;
    case GENERIC_TIMER_REG_KCTL:
        val = read_cntkctl();
        break;
    case GENERIC_TIMER_REG_HYP_CTRL:
        val = read_cnthp_ctl();
        break;
    case GENERIC_TIMER_REG_HYP_TVAL:
        val = read_cnthp_tval();
        break;
    case GENERIC_TIMER_REG_PHYS_CTRL:
        val = read_cntp_ctl();
        break;
    case GENERIC_TIMER_REG_PHYS_TVAL:
        val = read_cntp_tval();
        break;
    case GENERIC_TIMER_REG_VIRT_CTRL:
        val = read_cntv_ctl();
        break;
    case GENERIC_TIMER_REG_VIRT_TVAL:
        val = read_cntv_tval();
        break;
    default:
        uart_print("Trying to read invalid generic-timer register\n\r");
        break;
    }
    return val;
}

static inline void generic_timer_reg_write64(int reg, uint64_t val)
{
    switch (reg) {
    case GENERIC_TIMER_REG_HYP_CVAL:
        write_cnthp_cval(val);
        break;
    case GENERIC_TIMER_REG_PHYS_CVAL:
        write_cntp_cval(val);
        break;
    case GENERIC_TIMER_REG_VIRT_CVAL:
        write_cntv_cval(val);
        break;
    case GENERIC_TIMER_REG_VIRT_OFF:
        write_cntvoff(val);
        break;
    default:
        uart_print("Trying to write invalid generic-timer register\n\r");
        break;
    }
    isb();
}

static inline uint64_t generic_timer_reg_read64(int reg)
{
    uint64_t val;
    switch (reg) {
    case GENERIC_TIMER_REG_HYP_CVAL:
        val = read_cnthp_cval();
        break;
    case GENERIC_TIMER_REG_PHYS_CVAL:
        val = read_cntp_tval();
        break;
    case GENERIC_TIMER_REG_VIRT_CVAL:
        val = read_cntv_cval();
        break;
    case GENERIC_TIMER_REG_VIRT_OFF:
        val = read_cntvoff();
        break;
    default:
        uart_print("Trying to read invalid generic-timer register\n\r");
        break;
    }
    return val;
}

/** @brief Registers generic timer irqs such as hypervisor timer event
 *  (GENERIC_TIMER_HYP), non-secure physical timer event(GENERIC_TIMER_NSP)
 *  and virtual timer event(GENERIC_TIMER_NSP).
 *  Each interrup source is identified by a unique ID.
 *  cf. "Cortex™-A15 Technical Reference Manual" 8.2.3 Interrupt sources
 *
 *  DEVICE : IRQ number
 *  GENERIC_TIMER_HYP : 26
 *  GENERIC_TIMER_NSP : 30
 *  GENERIC_TIMER_VIR : 27
 *
 *  @note "Cortex™-A15 Technical Reference Manual", 8.2.3 Interrupt sources
 */
static hvmm_status_t generic_timer_init()
{
    _timer_irqs[GENERIC_TIMER_HYP] = 26;
    _timer_irqs[GENERIC_TIMER_NSP] = 30;
    _timer_irqs[GENERIC_TIMER_VIR] = 27;

    return HVMM_STATUS_SUCCESS;
}

/** @brief Configures time interval by PL2 physical timerValue register.
 *  Read CNTHP_TVAL into R0.
 *
 *  "CNTHP_TVAL" characteristics
 *  -Holds the timer values for the Hyp mode physical timer.
 *  -Only accessible from Hyp mode, or from Monitor mode when SCR.NS is  set to 1.
 */
static hvmm_status_t generic_timer_set_tval(uint32_t tval)
{
    hvmm_status_t result = HVMM_STATUS_UNSUPPORTED_FEATURE;

    if (_timer_type == GENERIC_TIMER_HYP) {
        _tvals[_timer_type] = tval;
        generic_timer_reg_write(GENERIC_TIMER_REG_HYP_TVAL, tval);
        result = HVMM_STATUS_SUCCESS;
    }

    return result;
}

/** @brief Enables the timer interrupt such as hypervisor timer event
 *  by PL2 Physical Timer Control register(VMSA : CNTHP_CTL)
 *  The Timer output signal is not masked.
 *
 *  The Cortex-A15 processor implements a 5-bit version of the interrupt
 *  priority field for 32 interrupt priority levels.
 */
static hvmm_status_t generic_timer_enable_int(void)
{
    uint32_t ctrl;
    hvmm_status_t result = HVMM_STATUS_UNSUPPORTED_FEATURE;

    if (_timer_type == GENERIC_TIMER_HYP) {
        ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
        ctrl |= GENERIC_TIMER_CTRL_ENABLE;
        ctrl &= ~GENERIC_TIMER_CTRL_IMASK;
        generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);
        result = HVMM_STATUS_SUCCESS;
    }

    return result;
}

/** @brief Disable the timer interrupt such as hypervisor timer event
 *  by PL2 physical timer control register.The Timer output signal is not masked.
 */
static hvmm_status_t generic_timer_disable_int(void)
{
    uint32_t ctrl;
    hvmm_status_t result = HVMM_STATUS_UNSUPPORTED_FEATURE;

    if (_timer_type == GENERIC_TIMER_HYP) {
        ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
        ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
        ctrl |= GENERIC_TIMER_CTRL_IMASK;
        generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);
        result = HVMM_STATUS_SUCCESS;
    }

    return result;
}


static void _generic_timer_hyp_irq_handler(int irq, void *regs, void *pdata)
{
    _callback[GENERIC_TIMER_HYP](regs);
}

/** @brief Enables generic timer irq such a s hypervisor timer event
 *  (GENERIC_TIMER_HYP)
 */
static hvmm_status_t generic_timer_enable_irq(void)
{
    hvmm_status_t result = HVMM_STATUS_UNSUPPORTED_FEATURE;

    if (_timer_type == GENERIC_TIMER_HYP) {
        uint32_t irq = _timer_irqs[_timer_type];
        if (interrupt_request(irq, &_generic_timer_hyp_irq_handler))
            return HVMM_STATUS_UNSUPPORTED_FEATURE;

        result = interrupt_host_configure(irq);
    }

    return result;
}

/** @brief Registers timer callback for each timer such as hypervisor timer
 *  event(GENERIC_TIMER_HYP), non-secure physical timer event(GENERIC_TIMER_NSP)
 *  and virtual timer event(GENERIC_TIMER_NSP).
 *  Each timer callback are registered with the timer interrput source.
 *  A timer callback called when the timer interrupt occurs.
 */
static hvmm_status_t generic_timer_set_callback(timer_callback_t callback,
                void *user)
{
    HVMM_TRACE_ENTER();
    _callback[_timer_type] = callback;
    HVMM_TRACE_EXIT();
    return HVMM_STATUS_SUCCESS;
}

/** @brief dump at time.
 *  @todo have to write dump with meaningful printing.
 */
static hvmm_status_t generic_timer_dump(void)
{
    HVMM_TRACE_ENTER();
    HVMM_TRACE_EXIT();
    return HVMM_STATUS_SUCCESS;
}

struct timer_ops generic_timer_ops = {
    .init = generic_timer_init,
    .request_irq = generic_timer_enable_irq,
    .free_irq = (void *)0,
    .enable = generic_timer_enable_int,
    .disable = generic_timer_disable_int,
    .set_interval = generic_timer_set_tval,
    .set_callbacks = generic_timer_set_callback,
    .dump = generic_timer_dump,
};

struct timer_module _timer_module = {
    .name = "K-Hypervisor Timer Module",
    .author = "Kookmin Univ.",
    .ops = &generic_timer_ops,
};
