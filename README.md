# midi\_uart\_lib
Library for using Raspberry Pi Pico UART1 or both UART0 and UART1 for serial port MIDI

Projects that use midi\_uart\_lib must contain a file called `midi_uart_lib_config.h`
that contains the following code

```
// Legal values for SERIAL_PORT_MIDI_NUM_UARTS:
// 1: assigns RP2040 UART1 to MIDI and does not assign UART0
// 2: assigns both RP2040 UART0 and UART1 to MIDI
// Any other value will cause a compiler error
// Note if you use both serial ports for MIDI then neither will
// be available for debugging
#define SERIAL_PORT_MIDI_NUM_UARTS 1

```

See the [midi2usbhost](https://github.com/rppicomidi/midi2usbhost) project for an
example of the `midi_uart_lib_config.h` file and for an example of how the library
is used.
