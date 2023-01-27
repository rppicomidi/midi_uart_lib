# midi\_uart\_lib
Library for using Raspberry Pi Pico UART1 or both UART0 and UART1 for serial port MIDI.

If you need more that two serial port MIDI ports, or if you need two serial port MIDI
ports and a stdio UART console, please consider using [pio_midi_uart_lib](https://github.com/rppicomidi/pio_midi_uart_lib) instead.

Projects that use midi\_uart\_lib must contain a file called `midi_uart_lib_config.h`
that contains the following code

```
// The standard MIDI UART baud rate is 31250. If you want to do
// something special with the MIDI UART LIB, you can change this
#define MIDI_UART_LIB_BAUD_RATE 31250
```

See the [midi2usbhost](https://github.com/rppicomidi/midi2usbhost) project for an
example of the `midi_uart_lib_config.h` file and for an example of how the library
is used.

It is up to the application that uses this library to "know" whether or not to
re-route or disable the stdio UART console (i.e `pico_enable_stdio_uart` in the `CMakeLists.txt` file) depending on which UART is used. By default, UART 0 is
the stdio UART. The Pico SDK lets you change it to UART 1 or disable it completely.

Also note that the UART TX pin for MIDI OUT is NOT open drain. For the projects I
have tried, it has not mattered. Some have reported needing to insert a diode between
the output pin and the MIDI OUT resistor to prevent poor signaling. If you are having
issues due to the UART TX pin's active high output, please consider using [pio_midi_uart_lib](https://github.com/rppicomidi/pio_midi_uart_lib) instead; the
PIO code for the serial transmitter creates an open drain output.
