Host measurement tools (2026-11-09, drafts). meas.c includes tests/hostsim.c: modes osc / raw / mix / trimdb / flt / np.
Build: cc -O2 -w -fpermissive -DYEHNAH_ANIMAL=1 -DYEHNAH_NOISE=1 -Ibuild/gen -Ifirmware/src -Itests tools/measure/meas.c -o $MEAS_DIR/meas -lm
For true pre-limiter levels build meas_nolim from a copy of firmware/ + tests/ with fx.c's `#define LIM_T 18000` made huge (iter.sh does this).
alias.py: aliasing (non-harmonic energy), flt.py: filter response, mixlvl.py: per-preset loudness through the mix,
apply_trims.py: lowers *_TRIM entries above a ceiling (-8 dBFS rms). Set MEAS_DIR to where meas / meas_nolim live.
