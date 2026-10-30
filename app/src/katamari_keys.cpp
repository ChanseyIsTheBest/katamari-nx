// katamari-nx (2026-10-30): the keys of banjo-nx files this port does not build, because their code is written for
// N&B's addresses (cpu/banjo_reverb_native.cpp, cpu/banjo_xaudio_native.cpp, cpu/banjo_hot_guest.cpp,
// renut_engine/hooks.cpp, renut_engine/frameHooks.cpp). banjo-nx's enhancement table (cpu/banjo_enhancements.cpp) and
// Renut's renderer still read them by name, so they are defined here, off: nothing in this build uses their code.
// Each comes back with its own file once it has been moved to Beautiful Katamari's functions (docs/katamari-notes.md).

#include <rex/cvar.h>

// N&B's XAudio natives (its reverb and sample-rate converter at N&B's addresses).
REXCVAR_DEFINE_BOOL(banjo_native_reverb, false, "Katamari/Not built", "N&B's native XAudio reverb (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_native_reverb_verify, false, "Katamari/Not built", "Verify mode of the above (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_native_xaudio_src, false, "Katamari/Not built", "N&B's native XAudio SRC (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_native_xaudio_src_verify, false, "Katamari/Not built", "Verify mode of the above (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// N&B's hand-written natives (cpu/hot/banjo_hand_natives.h, at N&B's addresses). Katamari's own come with its profile.
REXCVAR_DEFINE_BOOL(banjo_hand_memcpy, false, "Katamari/Not built", "N&B's hand-written memcpy (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_hand_memset, false, "Katamari/Not built", "N&B's hand-written memset (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_hand_constants, false, "Katamari/Not built", "N&B's hand-written constants push (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_hand_state_flush, false, "Katamari/Not built", "N&B's hand-written state flush (not in this port)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(banjo_hot_count, false, "Katamari/Not built", "Call counts of the native copies (not in this port yet)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// renut_engine/hooks.cpp's MSAA switch, which Renut's renderer reads (render/surfaces and the tuning): N&B's own key.
// Katamari's MSAA is what the game asks for.
REXCVAR_DEFINE_BOOL(disable_msaa, false, "Katamari/Graphics", "Draw the game's MSAA targets with one sample (not exact)");

// renut_engine/frameHooks.cpp's frame cap, which Renut's present and display_sync read.
REXCVAR_DEFINE_STRING(frame_cap, "Off", "Katamari/Performance", "Frame cap of Renut's present: Off, Display, 30, 60");
