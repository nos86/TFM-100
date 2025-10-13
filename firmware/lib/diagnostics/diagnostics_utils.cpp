#include "diagnostics.h"

// Layout flags
#define DTC_DEB_MASK 0x0Fu
#define DTC_STATE_MASK 0x70u
#define DTC_STATE_SH 4
#define DTC_RAW_MASK 0x80u

uint8_t dtc_get_debounce(const dtc_history_t *s)
{
    return s->flags & DTC_DEB_MASK;
}
void dtc_set_debounce(dtc_history_t *s, uint8_t deb)
{
    s->flags = (s->flags & ~DTC_DEB_MASK) | (deb & DTC_DEB_MASK);
}

dtc_state_t dtc_get_state(const dtc_history_t *s)
{
    return (dtc_state_t)((s->flags & DTC_STATE_MASK) >> DTC_STATE_SH);
}
void dtc_set_state(dtc_history_t *s, dtc_state_t st)
{
    s->flags = (s->flags & ~DTC_STATE_MASK) | (((uint8_t)st & 0x07u) << DTC_STATE_SH);
}

bool dtc_get_raw(const dtc_history_t *s)
{
    return (s->flags & DTC_RAW_MASK) != 0;
}
void dtc_set_raw(dtc_history_t *s, bool raw)
{
    if (raw)
        s->flags |= DTC_RAW_MASK;
    else
        s->flags &= ~DTC_RAW_MASK;
}
