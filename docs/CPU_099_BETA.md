# 0.9.9 CPU / polyphony beta

The cpu2 preview includes cpu1's USB/dual-core/POS fixes and the FM6 compact envelope and carrier-mask optimization (576 bytes less FM6 state; sample-exact reference tests).

## More voices, with measured admission

The per-track eight-slot arrays and the FM6/VOICE per-engine caps are unchanged. FULL may admit twelve simultaneous shared voices instead of eight after sixteen consecutive I2S half-buffers below 50% of their deadlines, including nested interrupt time. This requires a healthy enabled second core. Above 65%, or with LITE/ECO/single-core operation, extra admission closes. Closing admission does not immediately kill existing held notes; the original quiet-tail preference, protected bass/lead notes, smooth steals and 80% overload guard remain. LITE stays at eight; ECO at six. Twelve is an upper admission limit, not a hardware guarantee for every patch, effect combination or USB mode. A sudden chord can still exceed the deadline; the guard is deliberately retained.

## What Bigorna contributes

Reviewed nzimas/bigorna, tree 39a46bfbe4e85b5aaa1b61afcc9488beec7fbe92, especially tracks.c (eng_cost, per-voice measured costs, budget_guard and cost-sorted CPU1 dispatch) and audio_worker.h (ownership and timeout recovery). It supports making polyphony depend on available processing time and preserving ownership. Yehnah already measures per-track cost in mc_tk and splits dependent track groups. This change uses Yehnah's measured I2S deadline time for conservative admission. It does not transplant Bigorna's different engines, claim its performance, or implement its per-voice scheduler.

## Validation and limits

The new audio test holds two notes on each of six alternating FM6/VOICE tracks: all twelve gates survive without a voice steal. It checks admission hysteresis, nested IRQ load, closing admission without killing notes, LITE/ECO limits and single-core/disabled-core fallback. Golden audio remains unchanged where the expanded admission is not enabled. The FM6 reference state-machine comparison covers 16,777,216 blocks. Target compiler layout/instruction checks, engine, USB scheduler, audio and concurrent mixer tests must pass before publication.

Native timing is useful for relative investigation but is not FM1 utilization. Actual sustainable FM6/VOICE polyphony must still be checked on the device. Bulk RAM remains in delay, reverb, slicer and comb buffers; shortening those would change sound/features and is outside this patch.
