#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

#include "macro.S"


// ------ SECTION BSS  ------
.section            .bss

    .global   new_crc
new_crc:            .long 0

    .global   loop_counter
loop_counter:       .long 0

    .global   counter
counter:            .long 0

    .global   values_buffer
values_buffer:      .long 0
                    .long 0
                    .long 0
    .global   current_measurement
current_measurement:.long 0   // A lsb variable indicates the last measurement.

    .global   temp_conv
temp_conv:          .long 0
    .global   hum_conv
hum_conv:           .long 0

converted_value:    .long 0
buffer:             .long 0


// ------ SECTION DATA ------
.section    .data

    .set    ADDRESS_W_COM,  0x80
    .set    ADDRESS_R_COM,  0x81
    .set    TEMP_HOLD_COM,  0xe3
    .set    TEMP_NOHOLD_COM,0xf3
    .set    HUM_HOLD_COM,   0xe5
    .set    HUM_NOHOLD_COM, 0xf5
    .set    READ_REG_COM,   0xe7
    .set    WRITE_REG_COM,  0xe6
    .set    SOFT_RESET_COM, 0xfe

    .set    ACK_COM,        0x01
    .set    NO_ACK_COM,     0x00
    .set    START_COM,      0x01
    .set    REP_START_COM,  0x02
    .set    NO_START_COM,   0x00
    .set    STOP_COM,       0x01
    .set    NO_STOP_COM,    0x00
    .set    HOLD_COM,       0x01
    .set    NO_HOLD_COM,    0x00

    .set    REG_COM,        0x0e


// ------ SECTION TEXT ------
.section    .text

    .macro  order_write set_start set_command set_ack set_stop
        move    r2, start
        move    r0, \set_start
        st      r0, r2, 0
        move    r2, command
        move    r0, \set_command
        st      r0, r2, 0
        move    r2, ack
        move    r0, \set_ack
        st      r0, r2, 0
        move    r2, stop
        move    r0, \set_stop
        st      r0, r2, 0
    .endm

    .macro  order_read set_hold set_variable set_ack
        move    r2, hold_mode
        move    r0, \set_hold
        st      r0, r2, 0
        move    r2, ack
        move    r0, \set_ack
        st      r0, r2, 0
        move    r2, \set_variable
    .endm


    .global entry
entry:
    move    r3, loop_counter
    ld      r0, r3, 0
    jumpr   main_loop, 5, gt
    add     r0, r0, 1
    st      r0, r3, 0
main_loop:
    move    r3, stack_start

    move    r2, counter
    ld      r1, r2, 0
    add     r1, r1, 1
    st      r1, r2, 0

    order_write     START_COM       ADDRESS_W_COM   ACK_COM     NO_STOP_COM
    psr
    jump    i2c_write

    move    r2, current_measurement
    ld      r0, r2, 0
    and     r0, r0, 0x01
    jumpr   temp_measure, 1, eq
hum_measure:
    order_write     NO_START_COM    HUM_HOLD_COM    ACK_COM     NO_STOP_COM
    jump    run_measure
temp_measure:
    order_write     NO_START_COM    TEMP_HOLD_COM   ACK_COM     NO_STOP_COM
run_measure:
    psr
    jump    i2c_write

    order_write     REP_START_COM   ADDRESS_R_COM   ACK_COM     NO_STOP_COM
    psr
    jump    i2c_write
    order_read      HOLD_COM        msb_bits        ACK_COM
    psr
    jump    i2c_read
    order_read      NO_HOLD_COM     lsb_bits        ACK_COM
    psr
    jump    i2c_read
    order_read      NO_HOLD_COM     crc_bits        NO_ACK_COM
    psr
    jump    i2c_read
    psr
    jump    stop_con

    psr
    jump    convert_measurement

    psr
    jump    fill_the_array

    psr
    jump    check_values

end_main_loop:
    psr
    jump    delay

    jump    entry


wake_up:
    wake
    WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
    halt



/* Count to the 10 sec between measurements */
delay:
  //move    r0, 0x00
//rep_del:
    stage_rst
del_loop:
    wait    65535
    stage_inc 1
    jumps   del_loop, 12, lt //123
    //add     r0, r0, 1
    //jumpr   rep_del, 3, lt //12
    ret

/*
  After measurement we get three variable: lsb_bits, msb_bits and crc_bits. The code takes lsb and msb variables
  and connect them into a one value (lsh msb by 8 and connect with lsb). It also checks the last two bits of the value,
  which indicate what was measured.
  In the next step the values are converted into a readable temperature or humidity. The loop divides the measured value
  (by subtraction) by 373 (temp) or 524 (hum) which is s step by 1*c or 1%.
*/
convert_measurement:
/* connect lsb_bits and msb_bits */
    move    r2, lsb_bits        //load lsb_bits
    ld      r2, r2, 0
    and     r0, r2, 0x02        //check two first bit ( 10/ 00, hum/ temp)
    and     r2, r2, 0xfc        //set 00
    move    r1, msb_bits
    ld      r1, r1, 0
    lsh     r1, r1, 8
    or      r2, r1, r2          // r1 = MSB | LSB
    rsh     r0, r0, 1
/* Change readed lsb and msb bits into temp or hum */
    stage_rst
    jumpr   temp, 0, eq
hum:
    move    r1, hum_data
    st      r2, r1, 0
    move    r1, 3146
rep_1:
    stage_inc 1
    add     r1, r1, 524
    sub     r0, r2, r1          // [ msb lsb ] - ( 3146 + ( 524 * x ))
    jumpr   rep_1, 524, gt
    jump    next_step
temp:
    move    r1, temp_data
    st      r2, r1, 0
    move    r1, 2552
rep_2:
    stage_inc 1
    add     r1, r1, 373
    sub     r0, r2, r1
    jumpr   rep_2, 373, gt
/* Check how many times the loop was repeated. */
next_step:
    move    r1, 0x00
check_value_loop_1:
    add     r1, r1, 1
    stage_dec 1
    jumps   check_value_loop_1, 0, gt
    move    r2, converted_value
    st      r1, r2, 0
    ret


fill_the_array:
    move    r2, current_measurement   // We have to take two bits (xxxx xXXx) to indicate position into the array
    ld      r1, r2, 0
    and     r0, r1, 0x06
    rsh     r0, r0, 1                 // Now we have xxxx xxXX

    move    r2, values_buffer         // New measured value is loaded and stored into arra values_buffer
    move    r1, converted_value
    ld      r1, r1, 0
    add     r2, r2, r0
    st      r1, r2, 0

    move    r2, current_measurement
    ld      r1, r2, 0
    and     r0, r1, 0x06
    rsh     r0, r0, 1
    jumpr   increase_loop, 2, lt
    /* Reset 0000 XXX0 bits and set overflow flag 0000 X000 to 1 */
    and     r1, r1, 0xf1
    st      r1, r2, 0

    move    r1, 0x00
    stage_rst
    move    r2, values_buffer
repeat_addition_1:            // Add values from array (values_buffer)
    ld      r0, r2, 0
    add     r1, r0, r1
    add     r2, r2, 1
    stage_inc 1
    jumps   repeat_addition_1, 2, le
    stage_rst
    move    r2, 0x00
repeat_addition_2:            // Take value from array (values_buffer) and subtract... (70+71+70)-(70+70+70)
    add     r2, r2, r0
    stage_inc 1
    jumps   repeat_addition_2, 2, le
    sub     r0, r1, r2
    jumpr   save_value, 0, eq   // means that measurement was ok
    jump    end_main_loop
save_value:
    move    r2, current_measurement
    ld      r0, r2, 0
    or      r0, r0, 0x08        // set 0000 x000 to indicate that value into temp/hum_conv is new
    st      r0, r2, 0
    jump    end_fill_loop
    /* Increase 0000 0XX0 bits which indicate how many times loop (measurement) was repeated*/
increase_loop:
    add     r0, r0, 1
    lsh     r0, r0, 1
    and     r1, r1, 0xf9
    or      r1, r1, r0
    st      r1, r2, 0
end_fill_loop:
    ret


check_values:
    move    r2, current_measurement
    ld      r1, r2, 0
    and     r0, r1, 0x08        // if value is measured correctly
    and     r1, r1, 0xf7
    st      r1, r2, 0
    jumpr   end_check_values, 0, eq
    and     r0, r1, 0x01
    jumpr   toggle, 0, eq
    and     r1, r1, 0xfe
    jump    end_toggle
toggle:
    or      r1, r1, 0x01
end_toggle:
    st      r1, r2, 0

    move    r2, converted_value
    ld      r2, r2, 0
    jumpr   hum_was_measured, 0, eq
temp_was_measured:
    move    r1, temp_conv
    jump    next_step_check
hum_was_measured:
    move    r1, hum_conv
next_step_check:
    ld      r0, r1, 0
    sub     r0, r0, r2
    jumpr   end_check_values, 0, eq

    st      r2, r1, 0
jump_to_wakeup:
    jump    wake_up

end_check_values:
    ret

/*
check_crc:
  stage_rst
  move    r1, counter       // set imortant variables before counting
  move    r2, 0x0080        // reset counter (0x80)
  st      r2, r1, 0
  move    r1, buffer
  move    r2, lsb_bits
  ld      r2, r2, 0
  st      r2, r1, 0
  move    r1, msb_bits      // load msb_bits variable into r1 register
  ld      r1, r1, 0

rep_check:
  lsh     r1, r1, 1

  move    r2, buffer       // load lsb_bits into r2 to copy next bit
  ld      r2, r2, 0

  move    r0, counter      // load counter...
  ld      r0, r0, 0
  and     r0, r2, r0       // ... and check value indicated by counter
  jumpr   if_zero, 0, eq
  or      r1, r1, 0x0001   // 0x00000001 set lsb into msb_bits to 1
  jump    after_zero
if_zero:
  and     r1, r1, 0xFFFE   // 0x11111110 set lsb into msb_bits to 0
after_zero:
  and     r0, r1, 0x0100   // check 9 bit and store a value into r0
  and     r1, r1, 0x00ff   // change 9 bit to 0
  jumpr   to_counter, 0, eq
xor:
  move    r2, 0x0031       // r2 = 00110001

  and     r0, r1, r2
  add     r1, r1, r2
  sub     r1, r1, r0
  sub     r1, r1, r0
to_counter:
  move    r2, counter
  ld      r0, r2, 0
  jumps   if_no_status, 2, gt

  move    r0, 0x0800
  st      r0, r2, 0
  move    r2, buffer
  move    r0, 0x0000
  st      r0, r2, 0

  jump    last_step_crc
if_no_status:
  rsh     r0, r0, 1
  st      r0, r2, 0
last_step_crc:
  stage_inc 1
  jumps   rep_check, 16, lt

  move    r2, new_crc
  st      r1, r2, 0

  ret
*/
