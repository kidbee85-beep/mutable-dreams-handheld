# Mutable Instruments sources in Mutable Dreams for Trimui Brick

Origin: https://github.com/pichenettes/eurorack, commit `f4cfa7697095a28d292bff758beb51deb49b5a86` (archive SHA-256 `7bade44c4b3839c7e505a128e68896cf0b039849b8bc824a41e0051e1dfb016e`) and https://github.com/pichenettes/stmlib, commit `d18def816c51d1da0c108236928b2bbd25c17481` (archive SHA-256 `690e8ced93a767ae1e4d5dd579716f101f80ba66d76c072e16487719166bd2c3`), downloaded 13.11.2026.

Code by Emilie Gillet, MIT licence (header in every file). Two stmlib helpers are public domain (`utils/murmurhash3.h`) or free to use (`utils/crc32.h`, Gary S. Brown). Mutable Instruments is a registered trademark. These modules appear in Mutable Dreams under names of their own (PARTICLES, CROSSMOD, RANDOM) and are credited as their origin.

Only these folders were taken: clouds/dsp, clouds/resources.*, clouds/drivers/debug_pin.h, beads/dsp, beads/resources.*, beads/drivers/debug_pin.h, warps/dsp, warps/resources.*, warps/drivers/debug_pin.h, marbles/random, marbles/ramp, marbles/resources.*, marbles/drivers/debug_pin.h, and stmlib dsp, utils, fft, stmlib.h. Beads is used since 0.4.0 (SEEDS), Rings (rings/dsp, rings/resources.*) and Elements (elements/dsp, elements/resources.*) since 0.34.0 (the FX RACKs' RESONATOR, SYNTH RINGS and SYNTH ELEMENTS; -Drings=mi_rings -Delements=mi_elements beside the 2.1 engine's own parts), Grids (grids/resources.*, grids/pattern_generator.*: GPL-3.0-or-later, read only by tools/gen_grids.py at build time to make build/gen/grids_map.h) since 0.34.0, the complete Plaits since 0.6.0 (POLY; compiled with -Dplaits=mi_plaits beside the 2.1 engine's own Plaits parts, and with -include stdio.h because plaits/user_data.h prints in TEST builds).

Tides 2 (since 0.44.0, the TIDE of VOICES): from the same eurorack archive (downloaded again 14.11.2026, SHA-256 checked equal to the one above), only `tides2/poly_slope_generator.{h,cc}`, `tides2/ramp_generator.h`, `tides2/ramp_shaper.h`, `tides2/ramp/ratio.h`, `tides2/resources.{h,cc}`; unchanged; compiled with -Dtides=mi_tides. The bridge `brick_mi_tides.cpp` runs its PolySlopeGenerator looping at control range (GATES output mode, main output, shift 1) and scales its +-5 swing to 0..1.

## Changes

1. `clouds/dsp/window.h`, `Window::Start`: also sets `done_ = false` and `half_ = false`. Without this, a window that was done (all of them after `Init`) never plays again, and STRETCH (WSOLA) mode stays silent. Found with a sine test: before 0.0 RMS, after audible output.

2. `plaits/dsp/dsp.h`: `kSampleRate` and `kCorrectedSampleRate` set to 44100, as Mutable Dreams 2.1 does in its own Plaits copy, so the POLY voice is in tune at the engine's 44.1 kHz. (Plaits extracted on 13.11.2026 from the same eurorack archive: plaits/dsp, plaits/resources.*, plaits/user_data.h, plaits/drivers/debug_pin.h.)

3. `elements/drivers/debug_pin.h`: not taken from the archive but written for the port (empty `TIC`/`TOC`), as the other modules' debug pins are.

4. `clouds/dsp/grain.h`, `Grain::RenderEnvelope` (high quality): the window table is read with the gain held below 1.0 (0.99999). At a gain of exactly 1.0 (the middle of every grain, phase 1.0) `Interpolate(lut_window, 1.0, 4096)` read `lut_window[4097]`, one float past the 4097-entry table (AddressSanitizer, 14.11.2026). The sound is the same (the table's last value either way).

5. `clouds/dsp/pvoc/frame_transformation.cc`, `FrameTransformation::SetPhases` (SPECTRAL mode): the phase step times the pitch ratio, a float up to 4 x 65535, went straight to `uint16_t`, which C++ leaves undefined above 65535 (UndefinedBehaviorSanitizer, 15.11.2026; on x86 and ARM the results differ). Now through `int32_t` first, which wraps modulo 65536 as the phase arithmetic intends.

Also found by the sanitizers and left as they are (they do the intended thing on every compiler the port uses): `plaits/dsp/fm/dx_units.h:79` shifts a negative value left; `stmlib/fft/shy_fft.h` (lines 299, 480, 485) forms a pointer before its array and indexes back into it. The bridge `brick_mi_clouds.cpp` keeps every 0..1 parameter just below 1.0 (at dry/wet 1.0 Clouds read `lut_xfade_in[17]`, one past its end).

## Notes (no changes)

- Beads: without a seed input the module only makes grains with the SEED button latched; the bridge passes `SEED_BUTTON_STATE_LOCKED`. The tape media halve the rate inside, so blocks are even (12 frames).
- Beads, Clouds: the bridge never clears their memories in the audio thread (statics start zeroed; Reset/Init make old contents unreachable).
- Rings runs at 48 kHz inside; the bridge (`brick_mi_rings.cpp`) raises its note by 1.4671 semitones so it is in tune at 44.1 kHz, in blocks of 24. Elements runs at 32 kHz inside; `brick_mi_elements.cpp` resamples linearly to 44.1 kHz in blocks of 16.

6. `clouds/dsp/pvoc/stft.h` (0.71): the phase vocoder's FFT takes stmlib's own `LutPhasor` (cosines from a table, 8 KB) instead of `RotationPhasor` (each twiddle multiplied on from the last, kept small for the module's chip). Same ShyFFT, same passes; the twiddles are exact instead of accumulated. Spectral PARTICLES costs 28 M instructions a second less (237 → 209 M/s for the whole sound on the Mac). The output differs only by rounding (at most 3 of 32768 in a 6 s recording, −66 dB to the signal), from the more exact twiddles.

7. `clouds/dsp/correlator.cc`, `Correlator::EvaluateNextCandidate` (0.79): for a candidate on a word boundary (`offset_bits` 0) the next word was shifted right by 32. On the module's Cortex-M4 a register shift by 32 gives 0, as intended; on x86 and on AArch64 (the handhelds) the shift count is taken modulo 32, so the whole next word was ORed in and the match for those candidates came out wrong (UndefinedBehaviorSanitizer, 16.11.2026). Now that word is left out when `offset_bits` is 0 - the module's own behaviour.
