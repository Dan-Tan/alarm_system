#include "sdkconfig.h"

/* registers */
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/soc_ulp.h"

    .bss

    /* rtc io number to enter/exit deep sleep */
    .global io_switch_number
io_switch_number:
    .long 0
    
    /* state of on/off button */
    .global io_switch_state
io_switch_state:
    .long 0

    .text
    .global entry
entry:
    jump wake_up
    move r3, io_switch_number /* load io_switch_number address into r3 */
    ld r3, r3, 0 /* load io_switch_number value into r3 */

    move r0, r3
    rsh r0, r0, 4 /* check if greater then 16 */
    and r0, r0, 1
    jump read_io_high, eq

read_io_low:
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
    rsh r0, r0, r3
    jump read_done

read_io_high:
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 16, 2) /* read rtc_io 16,17 */
    sub r3, r3, 16 /* r3 contains the io_switch_number, convert to reading idx */
    rsh r0, r0, r3

read_done:
    move r3, io_switch_state
    add r0, r0, 0
    jump handle_inactive, eq

handle_pressed:
    ST r0, r3, 0
    jump entry

handle_inactive:
    ld r3, r3, 0
    sub r3, r3, 1 /* if sub is zero provides ALU condition to jump */
    jump wake_up, eq 
    jump entry

wake_up:
    READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
    and r0, r0, 1
    jump wake_up, eq

    /* Wake up the SoC, end program */
    wake
    halt
