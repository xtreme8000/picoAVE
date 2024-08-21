## picoAVE

A hardware/software HDMI mod designed for the Nintendo Wii based on the rp2040 microcontroller found on the Raspberry Pi Pico. Due to the readily available parts, this mod should be a lot cheaper to build compared to FPGA based solutions. It cost me less than 50€ to have two made, so only 25€ a piece. Sadly custom PCBs are necessary, as the stock pico board dues not bring out the required clock input pin and is also too large to install inplace.

The CPU runs a parallel video processing pipeline across both cores to achieve full 720x480p 60Hz throughput without any color/information loss or other compromises. There is vsync by design, so no tearing issues. A separate background task sends out audio packets in spare time.

### Building (software)

This is the same as any other pico project. You should probably edit the pico sdk path, too. It is absolutely crucial to build in release mode.
```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DPICO_SDK_PATH=/opt/pico-sdk/ ..
make
```

### Building (hardware)

TODO

### Flashing firmware

Best done with a Raspberry Pi or another Pico you have available. TODO

### Processing pipeline
**Video**
1. A PIO captures the [incoming video signal](https://wiibrew.org/wiki/Hardware/AV_Encoder#VData_Encoding) from the AV Encoder, which encodes a frame with NTSC timing and Ycbcr color space. There is a continous DMA transfer with two channels running as a ping-pong buffer. On completion of each transfer, the buffer is put in a queue for later processing.
2. Core 2 recieves completed buffers of video data from this queue. Then the visible display region is copied from the buffer and the cbcr-data is TMDS encoded. Once one display line is finished it is sent to core 1 using another queue.
3. Core 1 takes each display line and encodes the missing y1y2-data; the result is placed into a final queue. This runs at highest priority using a software-triggered (from core 2) interrupt.
4. At last there are 3 PIOs encoding a differential signal, which is supplied by 3 DMA transfers on ping-pong buffers. A DMA completion interrupt dynamically builds the HDMI signal, placing display lines and data packets when needed.

**Audio**
The processing is similar to video, except that solely core 1 encodes incoming audio frames to HDMI data packets at a lower priority.

### Images
![Wii menu frame](docs/images/frame.png)
> Example raw frame captured from the Wii home menu with a cheap HDMI capture card. There is a small overlay with information about the current input signal. It disappears after a few seconds.

![Original prototype](docs/images/prototype.webp)
> The original prototype, still based on an actual pico board. Because the CPU clock ran unsynchronized to the video input, the HDMI signal dropped every few seconds (pipeline ran out of data). There are two TXS0108E voltage shifters due to the 1.8V signal levels of the GPU. Below is a custom flex pcb soldered directly to the board's vias. The Wii2HDMI is used to force the Wii into 480p output mode.