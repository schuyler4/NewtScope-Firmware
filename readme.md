# NewtScope Firmware
This is firmware to make a 62.5MS/s oscilloscope from a Pi Pico. This is done by utilizing the PIO and DMA on the RP2040 chip to clock in data at a very high rate.  
Of course an analog front end and parallel ADC is also necessary. This is in the
works.
## Control
The oscilloscope system is controlled by a computer that the Pi Pico is connected to. This is done using basic serial over USB. The instrument control
software [voltpeek](https://github.com/schuyler4/voltpeek) provides a UI and Python API to control the oscilloscope. 
## Triggering
Currently there are two trigger modes, force trigger and normal trigger. Force trigger takes a snapshot of the signal when initiated, and normal trigger waits for an edge to capture. Currently with normal trigger the entire capture occurs after the trigger event. That is, the trigger is at the start of the capture. Center triggering is still in progress.     