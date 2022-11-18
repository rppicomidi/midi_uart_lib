/**
 * @file midi_uart_lib.c
 * @brief this library provides functions for using a Raspberry Pi Pico
 * UART as a MIDI interface
 * 
 * MIT License

 * Copyright (c) 2022 rppicomidi

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"
#include "pico/sync.h"
#include "ring_buffer_lib.h"
#include "midi_uart_lib_config.h"
#include "midi_uart_lib.h"

#define MIDI_UART_RING_BUFFER_LENGTH 128
typedef struct MIDI_UART_S {
    // UART selection and pin mapping.
    uart_inst_t *midi_uart;
    uint midi_uart_irq;
    uint midi_uart_tx_gpio;
    uint midi_uart_rx_gpio;

    // UART ring buffers; the application does not know about these
    ring_buffer_t midi_uart_rx, midi_uart_tx;
    uint8_t midi_uart_rx_buf[MIDI_UART_RING_BUFFER_LENGTH];
    uint8_t midi_uart_tx_buf[MIDI_UART_RING_BUFFER_LENGTH];
} MIDI_UART_T;

#ifndef MIDI_UART_LIB_NUM_UARTS
#error "projects that use this library must define MIDI_UART_LIB_NUM_UARTS in midi_uart_lib_config.h"
#endif
#ifndef MIDI_UART_LIB_BAUD_RATE
#error "projects that use this library must define MIDI_UART_LIB_BAUD_RATE in midi_uart_lib_config.h"
#endif

static void on_midi_uart_irq(MIDI_UART_T *midi_uart);
#if MIDI_UART_LIB_NUM_UARTS==1
static MIDI_UART_T midi_uart1;
#elif MIDI_UART_LIB_NUM_UARTS==2
static MIDI_UART_T midi_uart0, midi_uart1;

static void on_midi_uart0_irq()
{
    on_midi_uart_irq(&midi_uarts0);
}
#else
#error "NEED EITHER 1 OR 2 MIDI_UART_LIB_NUM_UARTS"
#endif

static void on_midi_uart1_irq()
{
    on_midi_uart_irq(&midi_uart1);
}

static void on_midi_uart_irq(MIDI_UART_T *midi_uart)
{
    uint32_t status = uart_get_hw(midi_uart->midi_uart)->mis; // the reason this irq happened
    if (status & UART_UARTMIS_RXMIS_BITS) {
        // Got a byte
        uint32_t dr = uart_get_hw(midi_uart->midi_uart)->dr;
        uint8_t val = dr & 0xff;
        if ((dr & !0xf00) == 0) {
            // then data read was OK.
            // ignore the return value. If the buffer is full, nothing can be done
            (void)ring_buffer_push_unsafe(&midi_uart->midi_uart_rx, &val, 1);
        }
    }
    if (status & UART_UARTMIS_TXMIS_BITS) {
        // ready to transmit
        uint8_t val;
        RING_BUFFER_SIZE_TYPE npopped = ring_buffer_pop_unsafe(&midi_uart->midi_uart_tx, &val, 1);
        if (npopped == 0) {
            // No data to send; disable the interrupt
            uart_set_irq_enables(midi_uart->midi_uart, true, false);
        }
        else {
            // send the byte
            uart_get_hw(midi_uart->midi_uart)->dr = val;
        }
    }
}

void *midi_uart_configure(uint8_t uartnum, uint8_t txgpio, uint8_t rxgpio)
{
#if MIDI_UART_LIB_NUM_UARTS == 2
    assert(uartnum < 2);
    midi_uart0.midi_uart = uart0;
    midi_uart0.uart_irq = UART0_IRQ;
#else
    assert(uartnum == 1);
#endif
    midi_uart1.midi_uart = uart1;
    midi_uart1.midi_uart_irq = UART1_IRQ;

    MIDI_UART_T *midi_uart;
    if (uartnum == 1) {
        midi_uart = &midi_uart1;
        irq_set_exclusive_handler(midi_uart->midi_uart_irq, on_midi_uart1_irq);
    }
#if MIDI_UART_LIB_NUM_UARTS == 2
    else if (uartnum == 0) {
        midi_uart = &midi_uart0;
        irq_set_exclusive_handler(midi_uart->midi_uart_irq, on_midi_uart1_irq);
    }
#endif
    midi_uart->midi_uart_tx_gpio = txgpio;
    midi_uart->midi_uart_rx_gpio = rxgpio;
    // Set up the UART hardware
    uart_init(midi_uart->midi_uart, MIDI_UART_LIB_BAUD_RATE);
    uart_set_format(midi_uart->midi_uart, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(midi_uart->midi_uart, false, false);
    uart_set_fifo_enabled(midi_uart->midi_uart, false);
    uart_set_translate_crlf(midi_uart->midi_uart, false);

    gpio_set_function(midi_uart->midi_uart_tx_gpio, GPIO_FUNC_UART);
    gpio_set_function(midi_uart->midi_uart_rx_gpio, GPIO_FUNC_UART);

    // Prepare the MIDI UART ring buffers and interrupt handler and enable interrupts
    ring_buffer_init(&midi_uart->midi_uart_rx, midi_uart->midi_uart_rx_buf, MIDI_UART_RING_BUFFER_LENGTH, midi_uart->midi_uart_irq);
    ring_buffer_init(&midi_uart->midi_uart_tx, midi_uart->midi_uart_tx_buf, MIDI_UART_RING_BUFFER_LENGTH, midi_uart->midi_uart_irq);

    irq_set_enabled(midi_uart->midi_uart_irq, true);

    // enable the rx IRQ. No data to send yet.
    uart_set_irq_enables(midi_uart->midi_uart, true, false);
    return midi_uart;
}

uint8_t midi_uart_poll_rx_buffer(void *instance, uint8_t* buffer, RING_BUFFER_SIZE_TYPE buflen)
{
    MIDI_UART_T *midi_uart = (MIDI_UART_T *)instance;
    return ring_buffer_pop(&midi_uart->midi_uart_rx, buffer, buflen);
}

uint8_t midi_uart_write_tx_buffer(void* instance, uint8_t* buffer, RING_BUFFER_SIZE_TYPE buflen)
{
    MIDI_UART_T *midi_uart = (MIDI_UART_T *)instance;
    return ring_buffer_push(&midi_uart->midi_uart_tx, buffer, buflen);
}

void midi_uart_drain_tx_buffer(void* instance)
{
    MIDI_UART_T *midi_uart = (MIDI_UART_T *)instance;
    // disable UART interrupts because checking if still transmitting from the buffer
    irq_set_enabled(midi_uart->midi_uart_irq, false);
    // can use the unsafe version of ring_buffer_is_empty_unsafe() because UART IRQ
    // is disabled
    if (!ring_buffer_is_empty_unsafe(&midi_uart->midi_uart_tx)) {
        uint8_t val;
        if ((uart_get_hw(midi_uart->midi_uart)->imsc & UART_UARTIMSC_TXIM_BITS) == 0) {
            // then last transmission is complete. Kick start a new one
            RING_BUFFER_SIZE_TYPE result = ring_buffer_pop_unsafe(&midi_uart->midi_uart_tx, &val, 1);
            assert(result == 1);
            uart_get_hw(midi_uart->midi_uart)->dr = val;
            uart_set_irq_enables(midi_uart->midi_uart, true, true);
        }
    }
    irq_set_enabled(midi_uart->midi_uart_irq, true);
}
