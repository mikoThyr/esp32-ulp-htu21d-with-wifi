#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

#include "macro.S"

// ------ SECTION BSS ------
.section    .bss

  .global stack_start
stack_start:
  .skip   32
  .global   stack_end
stack_end:  .long 0

  .global   hum_data
hum_data:   .long 0
  .global   temp_data
temp_data:  .long 0

// ------ SECTION DATA ------
.section    .data

  .global   msb_bits
msb_bits:   .long 0
  .global   lsb_bits
lsb_bits:   .long 0
//  .global   crc_bits
//crc_bits:   .long 0
  .global   start
start:      .long 0
  .global   command
command:    .long 0
  .global   ack
ack:        .long 0
  .global   stop
stop:       .long 0
  .global   hold_mode
hold_mode:  .long 0



// ------ SECTION TEXT ------
.section    .text

  .global   i2c_read
  .global   i2c_write
  .global   stop_con

  .macro read_SDA
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 0x06, 1)
  .endm

  .macro  set_SDA
    WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 0x06, 1, 1)
  .endm

  .macro  set_SCL
    WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 0x07, 1, 1)
  .endm

  .macro  clr_SDA
    WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 0x06, 1, 1)
  .endm

  .macro  clr_SCL
    WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S + 0x07, 1, 1)
  .endm


i2c_read:
  STAGE_RST
  move    r0, hold_mode
  ld      r0, r0, 0
  move    r1, 0x00
  jumpr   write_buffer, 0, eq
check_hold_line:
  set_SCL
  READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 0x07, 1)
  jumpr   after_hold, 1, eq
  jump    check_hold_line

write_buffer:
  wait    15
  set_SCL
  lsh     r1, r1, 1
after_hold:
  READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 0x06, 1)
  or      r1, r1, r0
  stage_inc 1
  clr_SCL
  jumps   write_buffer, 8, lt
  st      r1, r2, 0
check_ack:
  move    r0, ack
  ld      r0, r0, 0
  jumpr   if_no_ack, 0, eq
if_ack:
  clr_SDA
  wait    5
  set_SCL
  wait    25
  clr_SCL
  set_SDA
  jump    to_rep
if_no_ack:
  set_SCL
  wait    15
  clr_SCL
  clr_SDA
to_rep:
  ret


i2c_write:
  move    r2, start
  ld      r0, r2, 0
  jumpr   if_no_start, 0, eq
  jumpr   if_start, 1, eq
if_rep_start:
  set_SDA
  set_SCL
if_start:
  clr_SDA
  clr_SCL
  jump    command_bits
if_no_start:
  clr_SDA
command_bits:
  stage_rst
  move    r2, command
  ld      r2, r2, 0
  move    r1, 0xc0
first_step:
  and     r0, r2, r1
  jumpr   second_step, 0x80, ge
  jumpr   LL, 0x00, eq
LH:
  clr_SDA
  set_SCL
  wait    10
  clr_SCL
  clr_SDA
  jump    end_condition
LL:
  clr_SDA
  set_SCL
  wait    10
  clr_SCL
  clr_SDA
  jump    end_condition
second_step:
  jumpr HL, 0x80, eq
HH:
  set_SDA
  set_SCL
  wait    10
  clr_SCL
  set_SDA
  jump    end_condition
HL:
  set_SDA
  set_SCL
  wait   10
  clr_SCL
  clr_SDA
  jump    end_condition
end_condition:
  STAGE_INC 1
  lsh     r2, r2, 1
  jumps   first_step, 8, lt
ack_bit:
  set_SDA
  set_SCL
  READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 0x06, 1)
  wait    5
  clr_SCL
  clr_SDA
  jumpr   no_ack, 1, eq
stop_bit:
  move    r2, stop
  ld      r0, r2, 0
  jumpr   stop_con, 1, eq
  set_SDA
  jump    end_stop
stop_con:
  set_SCL
  set_SDA
end_stop:
  ret


no_ack:
  stage_rst
  set_SCL
  set_SDA
no_ack_loop:
  wait    65535
  stage_inc 1
  jumps   no_ack_loop, 255, lt
  jump    entry
