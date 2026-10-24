# Reference video line handoff audit

The machine accepts an optional caller callback through
`mipsnes2_machine_config.video_line` and `video_context`. At the provisional
HBlank line event, it supplies the visible line number and 256 nominal BGR555
pixels after layer, window, color-math and brightness processing. Forced
blank supplies a black line. The pointer belongs to the machine and is valid
only during the callback; a PS2 or PSP backend must copy it before returning.
No PS2 GS, PSP GU or filesystem API enters `core/`.

The callback is a synchronous observation point, not a final presentation
contract. The current line is a whole-line snapshot of PPU state at the
scheduled event. Mid-line writes, exact scanout phase, interlace, hires,
overscan presentation, scaling, buffering, VSync and frame pacing remain
open. A consumer must not reenter the machine from inside the callback.

`make clean && make -j2 test` uses a synthetic machine to verify line 0's
BGR555 pixel, ordered line 1 delivery, forced blank and that the consumer's
copied prior line is stable when the machine reuses its line buffer. Target
build/CI evidence is recorded in [STATUS.md](../STATUS.md). No actual GS/GU
rendering or console runtime is claimed.
