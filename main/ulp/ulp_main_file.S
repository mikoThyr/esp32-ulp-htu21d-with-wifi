#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

#include "macro.S"


// ------ SECTION BSS  ------
.section            .bss

  .global   counter
counter:            .long 0

  .global   values_buffer
values_buffer:      .long 0
                    .long 0
                    .long 0
  .global   temperature_pointer
temperature_pointer:.long 0
temperature_buffer: .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0

  .global   humidity_pointer
humidity_pointer:   .long 0
humidity_buffer:    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0
                    .long 0

  .global   current_measurement
current_measurement:.long 0   // A lsb variable indicates the last measurement.

  .global   temp_conv
temp_conv:          .long 0   // Converted temperature value from the sensor numbers
  .global   hum_conv
hum_conv:           .long 0   // Converted humidity value from the sensor numbers
numerator:          .long 0   // if array of measured values is full this variable store the rest of the dividing
converted_value:    .long 0   // msb + lsb
buffer:             .long 0

  .global   wake_sw
wake_sw:            .long 0


// ------ SECTION DATA ------
.section    .data

    .set    ADDRESS_W_COM,  0x80
    .set    ADDRESS_R_COM,  0x81
    .set    TEMP_HOLD_COM,  0xe3
    .set    HUM_HOLD_COM,   0xe5
    .set    READ_REG_COM,   0xe7
    .set    WRITE_REG_COM,  0xe6

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
    move    r3, stack_start

    move    r2, wake_sw
    move    r1, 0x00
    st      r1, r2, 0
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 0x11, 1)
    jumpr   wake_push, 1, eq

    order_write     START_COM       ADDRESS_W_COM   ACK_COM     NO_STOP_COM
    psr
    jump    i2c_write

    move    r2, current_measurement
    ld      r0, r2, 0
    and     r0, r0, 0x01
    jumpr   temp_measure, 0, eq
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
    order_read      NO_HOLD_COM     lsb_bits        NO_ACK_COM
    psr
    jump    i2c_read
//    order_read      NO_HOLD_COM     crc_bits        NO_ACK_COM
//    psr
//    jump    i2c_read
    psr
    jump    stop_con

    psr
    jump    convert_measurement

    psr
    jump    fill_the_array

    psr
    jump    add_to_table

    psr
    jump    check_values

end_main_loop:
    psr
    jump    delay

    jump    entry

wake_push:
    move    r2, wake_sw
    move    r1, 0x01
    st      r1, r2, 0
wake_up:
    /*INFO: There will be no wakeup if counter is lower than 20.*/
    move    r3, counter
    ld      r0, r3, 0
    jumpr   run_program, 1, ge
    add     r0, r0, 1
    st      r0, r3, 0
    jump    entry
run_program:
    wake
    WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
    halt



/* Count to the 10 sec between measurements */
delay:
  //move    r0, 0x00
// rep_del:
    stage_rst
del_loop:
    wait    40000
    stage_inc 1
    jumps   del_loop, 10, lt //123
    // add     r0, r0, 1
    // jumpr   rep_del, 3, lt //12
    ret

/*INFO:
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
    and     r0, r2, 0x02        //check two first bit ( 00/ 10, hum/ temp)
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
    or      r0, r0, 0x08        // set 0000 x000 to indicate that value into temp/hum_conv is new and ready to compare with old
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

// INFO: function to add values to table and take average value.
add_to_table:
  move    r2, current_measurement     // INFO: check the flag bit indicating that the converting step was ok
  ld      r1, r2, 0
  and     r0, r1, 0x08
  and     r1, r1, 0xe7                // reset the flag 1111 0111
  st      r1, r2, 0
  jumpr   end_add, 0, eq
  and     r0, r1, 0x01                // check what was measured
  jumpr   toggle, 0, eq
  and     r1, r1, 0xfe
  jump    end_toggle
toggle:
  or      r1, r1, 0x01
end_toggle:
  st      r1, r2, 0
  move    r2, converted_value
  ld      r2, r2, 0
  jumpr   temp_measurement, 0, eq
hum_measurement:
  move    r0, humidity_pointer        // INFO: store the converted value in a temperature_buffer array
  ld      r0, r0, 0
  move    r1, humidity_buffer
  add     r1, r1, r0
  st      r2, r1, 0
  move    r2, humidity_pointer        // INFO: increase a temperature_pointer variable by 1
  add     r0, r0, 1
  st      r0, r2, 0
  jumpr   end_add, 20, lt
  move    r1, humidity_buffer
  jump    after_load
temp_measurement:
  move    r0, temperature_pointer     // store the converted value in a temperature_buffer array
  ld      r0, r0, 0
  move    r1, temperature_buffer
  add     r1, r1, r0
  st      r2, r1, 0
  move    r2, temperature_pointer     // increase a temperature_pointer variable by 1
  add     r0, r0, 1
  st      r0, r2, 0
  jumpr   end_add, 20, lt
  move    r1, temperature_buffer
after_load:
  sub     r0, r0, 1
  st      r0, r2, 0
  ld      r2, r1, 0
  stage_rst
count_rep_3:                          // INFO: do the addition on the variables in the array
  add     r0, r0, r2
  add     r1, r1, 1
  ld      r2, r1, 0
  sub     r1, r1, 1
  st      r2, r1, 1
  add     r1, r1, 1
  stage_inc 1
  jumps   count_rep_3, 20, lt
  move    r1, 0x00
count_rep_4:
  sub     r0, r0, 20
  add     r1, r1, 1
  jumpr   count_rep_4, 20, ge
  move    r2, converted_value
  st      r1, r2, 0
  move    r2, numerator
  st      r0, r2, 0
  move    r2, current_measurement
  ld      r1, r2, 0
  or      r1, r1, 0x10
  st      r1, r2, 0
  jump    end_add
end_add:
  ret

check_values:
  move    r2, current_measurement
  ld      r1, r2, 0
  and     r0, r1, 0x10        // if value is measured correctly
  jumpr   end_check_values, 0, eq
  and     r1, r1, 0xef
  st      r1, r2, 0
  move    r1, converted_value
  ld      r1, r1, 0
  move    r0, numerator
  ld      r0, r0, 0
  jumpr   step_1, 8, gt
  jump    step_2
step_1:
  add     r1, r1, 1
step_2:
  ld      r0, r2, 0
  and     r0, r0, 0x01
  jumpr   if_hum, 0, eq
  move    r2, temp_conv
  sub     r1, r1, 0x28        // 0x28 = 40
  jump    step_3
if_hum:
  move    r2, hum_conv
step_3:
  ld      r0, r2, 0
  sub     r0, r1, r0
  jumpr   end_check_values, 0, eq
  st      r1, r2, 0
  jump    wake_up

end_check_values:
  ret
