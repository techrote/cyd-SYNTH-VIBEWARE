# Hardware and resource contracts

## Initial target and reference evidence

Classic ESP32-2432S028R (Cheap Yellow Display), landscape 320×240; single-contact resistive touchscreen. The intended reference profile is ILI9341 LCD, XPT2046 controller, original ESP32 and 4 MB flash as documented by [Diviner](https://github.com/techrote/cyd-diviner/blob/main/README.md). Actual board revision, SPI pins/buses, SD wiring, USB bridge, speaker circuit, flash and PSRAM must be verified; do not silently apply this to different CYD variants. Do not assume PSRAM. Validate rotation and pin mapping before enabling outputs.

[Diviner PlatformIO](https://github.com/techrote/cyd-diviner/blob/main/platformio.ini) currently documents esp32dev, Arduino, espressif32 7.1.3, LovyanGFX 1.2.32 and XPT2046 v1.4. Treat these as discovery references, pin reviewed exact revisions and verify compatibility independently. cyd-MHAC #2/#4 provide candidate 24 kHz DSP, display and touch work, still Draft/HOLD at planning.

## Audio routes

- GPIO26 internal DAC / board speaker path: verify actual peripheral API, DAC data format, output midpoint, startup/shutdown transient behavior, amplifier/pin configuration and recovery. No output on arbitrary GPIO. Start with conservative gain and reachable mute.
- USB serial PCM: Diviner documents 533333 baud 8N1, signed 16-bit LE mono 24 kHz, 128 samples/frame, CYD1 marker, sequence and CRC-16, start/ping/stop protocol and Windows VLC input module. A valuable candidate, not proof that all bridges or firmware profiles support it. At that rate nominal payload capacity is ~53.3 kB/s; 48 kB/s raw PCM plus framing leaves limited margin. Measure throughput, missing frames, backpressure and concurrent control. Do not block audio producer on serial. Verify compatibility before claiming Diviner protocol support.
- Optional external I2S audio only as a separately verified hardware profile, no assumption of spare pins or installation. Desktop PCM is the digital mix, not evidence of speaker quality.

## Display and memory

320×240×2 = 153600 bytes (150 KiB) RGB565 framebuffer; double buffering = 307200 bytes (300 KiB). Do not allocate full double buffers by default. Example strip 320×16×2 = 10240 bytes (10 KiB); two strips = 20 KiB. Example 160×120 indexed history = 19200 bytes (18.75 KiB), plus palette and scratch. These are arithmetic estimates, not free heap measurements.

Budget candidate strip/tile buffers, touch and audio buffers, simulation pools, RTOS stacks, static data, largest contiguous heap block, and Wi-Fi/BLE dynamic allocations. On target record map sections, flash partition fit, actual heap/minimum/largest block, stack high-water marks and timing; if no device available mark runtime measurements NOT RUN. Prefer internal DMA-capable buffers and explicit completion ownership. Verify actual SPI speed, transfer windows and bus arbitration with touch/SD. No invented fps figures.

## Performance targets (provisional, not acceptance claims)

- Reference audio candidate: 24 kHz mono, 128-sample block (~5.33 ms), no underrun in sustained stress after startup. Measure worst-case callback time and slack; 70% utilization or less is an initial engineering goal, not a substitute for actual deadline qualification.
- Aim for responsive interaction: 20–30 fps complex scenes when feasible, faster simple scenes; visible quality degradation before audio failure. Log frame/strip times, dropped updates and touch-to-control/display latency. Revise targets using actual device measurements and owner experience; report exact workload.
- No unbounded alloc/queue, NaN/Inf or unsafe saturation in real-time; no repeated flash writes in callback. Native tests do not demonstrate target stack/heap timing.

## Touch, networking and storage

Single touch; calibrated min/max, rotation and inversion, debouncing, no-contact lease, release on loss, explicit touch cancel; do not infer reliable pressure sensitivity. Fullscreen escape and panic/mute must remain accessible. Use native mouse or scripted contact for replay, label simulated.

Wi-Fi/BLE/SD are opt-in capabilities; verify target revision, firmware/driver support, pins, peak heap and concurrency. Disable unneeded radios by default. No assumed BLE-MIDI, USB MIDI or native USB audio (USB-UART bridge is not USB device audio). UDP/OSC control rate limits, reconnect, stale message policy, pairing and offline fallback. SD optional and not required to boot/play; validate lengths and formats.

## Physical acceptance

Before owner flashing: exact board/flash ID, backed-up original firmware with verified readable hashes, documented supported restore/bootloader method, compatible partition/offsets and tool versions, safe outputs, explicit owner approval. Supply instructions separately from any automated CI. Do not bundle an installable image as approved for use without these checks and rights review. Record actual board revision, setup, performance/audio/control observations and raw diagnostic evidence. Never erase, flash or probe an owner's device unattended.
