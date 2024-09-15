## Processing pipeline
#### Video
1. A PIO captures the [incoming video signal](https://wiibrew.org/wiki/Hardware/AV_Encoder#VData_Encoding) from the AV Encoder, which encodes a frame in NTSC timing and Ycbcr color space. There is a continous DMA transfer with two channels running as a ping-pong configuration. On completion of each transfer, its data buffer is put in a queue for later processing.
2. Core 2 recieves completed buffers filled with video data from this queue. Every visible display region is copied from a buffer and the cbcr-data is TMDS encoded. A pre-sync step (run once in advance) determines which data regions are visible video data and need to be extracted. Once one display line is finished, it is sent to core 1 using another queue.
3. Core 1 takes each display line and encodes the missing y1y2-data; the result is placed into a final queue. This step runs at highest priority using a software-triggered interrupt, initiated by core 2.
4. At last there are 3 PIOs, each encoding one differential signal lane, and are supplied by 3 DMA transfers on ping-pong buffers. A DMA completion interrupt builds the HDMI signal at run time; placing display lines and data packets as needed.

#### Audio
The processing is similar to video, except that solely core 1 encodes incoming audio frames to HDMI data packets at a lower priority.

## TMDS encoding

Video data is arranged as 32-bit words, each containing 2 pixels as `(y1, cb, y2, cr)` tuples (Ycbcr color space, each element 8-bits). Encoding is a two step process, first `tmds_encode_y1y2()` encodes `(y1, y2)` into one TMDS-lane, then `tmds_encode_cbcr()` the color part `(cb, cr)` separately. For cbcr it simply shifts the input data before processing, but then continues exactly the same. Both implementations are unrolled to process 6 pixels at once, so necessary loads and stores can be combined to save cycles. The (assembly) code can be found in [`tmds_encode.c`](../src/tmds_encode.c).

#### TMDS LUTs
There are two lookup tables (LUT) for TMDS generation. Each table contains 32-bit words of `(bias, symbol)`. The (following) bias consists of 4-bits stored in the MSBs, as TMDS encoding only ever uses 9 different bias values. Each lookup requires a pixel value and the preceding bias as address, then giving the following bias and TMDS encoded symbol. The symbol is either stored at bit offset 0 or 10 depending on the lookup table. The idea is that LUT 1 contains the first pixel and LUT 2 the second. Because the differential output stage (realized by a PIO) requires a sequential stream of bits on the lower 20-bits of each word, the words from both LUTs can then be simply OR-ed together to receive the final output for both pixels. Both lanes of an interpolator are used to compute the lookup addresses.

#### Interpolator configuration

The interpolator receives a pixel pair `(y1, cb, y2, cr)`.

* Lane 0 extracts y2 and shifts and masks that 8-bit part into bits `[9:2]`. Finally the LUT 1 address is added. LUT 1 is indexed by `bias * 256 + y2`.
* Lane 1 extracts y1 and moves those to bits `[13:6]`, then the LUT 2 base address is added. LUT 2 is indexed by `y1 * 16 + bias`.

Note, how both lanes are different, especially in their memory layout. Ideally both LUTs would use the memory layout of LUT 1, because it wastes no space (remember only 9 of 16 biases are used) and distributes loads across all four SRAM banks evenly, however the layout of each LUT word `(bias, symbol)` prevents this: To save cycles the bias is added onto the interpolator output by the memory load operation itself, as in the following example.

```arm
ldr %%r7, [%[interp], #0x24]  // get result from interpolator
ldr %%r7, [%%r7, %[bias]]     // load from lookup table
```

The bias offset needs to be naturally aligned (4 bytes) and is right shifted after each lookup from the MSBs down to the correct position, for the final address addition. Thus in layout of LUT 1 we need at least 10 empty padding (0) bits between bias and symbol in a LUT word (8 for `bias * 256` and 2 for alignment). This is fine for the layout of the first pixel `(bias, 18, symbol)`, but is an issue with `(bias, 8, symbol, 10)` of the second pixel, which would then violate natural alignment, because of garbage data in the lowest 2 bits (masking these would require another cycle which should be avoided).

#### Processing steps

1. The pixel pair is sent to the interpolator.
2. The first pixel base offset is retrieved, then the LUT 1 word is loaded from this offset and the current bias.
3. The current bias is updated by extracting the "following" bias from the LUT word. It is shifted to the right position for the next lookup.
4. The second pixel base offset is retrieved, then the LUT 2 word is loaded from this offset and the current bias.
5. The current bias is updated as before.
6. Both LUT words are OR-ed together to receive the final output.