#!/usr/bin/env python3
"""katamari-nx (2026-10-30): writes the package's tomls from banjo-nx's shipped renut.toml (its SD package of
2026-10-29 evening), with Beautiful Katamari's settings:

  renut.toml       the shipped settings: Renut on deko3d, pixel-identical (720 lines in both modes, no dynamic
                   resolution, the game's own MSAA), every exact enhancement as banjo-nx ships it, the items measured
                   SLOWER ON THE CONSOLE off, diagnostics as banjo-nx's renut.toml (logs on);
  renut_play.toml  renut.toml with the render-thread diagnostics off (banjo-nx's renut_play.toml difference);
  renut_ab.toml    renut.toml with an in-run A/B list, 30 s halves, target 1 (a Vulkan-only key: does nothing on the
                   Switch) as the control.
  configs/*.toml   the picture-changing options, each one key away from renut.toml, for the user to decide on.

  python tools/make_katamari_tomls.py --banjo <banjo-nx renut.toml> --out dist/switch/katamari-nx
"""
import argparse
import os
import re

HEADER = """# katamari-nx (Beautiful Katamari on the Switch, Renut's renderer on deko3d): {what}
# Made by tools/make_katamari_tomls.py from banjo-nx's shipped renut.toml (2026-10-29). Copy to
# sdmc:/switch/katamari-nx/renut.toml. Credits: reNut's renderer (masterspike52, reuse permitted), banjo-nx, sa2-nx,
# NetherSX2_nx (four-core layout), battd_nx (the GPU arena), bejeweled3-nx and the zuma ports (hot functions).
"""

# Keys set for Beautiful Katamari (the rest as banjo-nx ships them).
SHIPPED = {
    'banjo_renderer': '"renut"',
    'renut_gpu_backend': '"deko3d"',
    # Pixel-identical by default: the console's 720 lines, the game's MSAA, no dynamic resolution.
    'banjo_renut_render_height_handheld': '720',
    'banjo_renut_render_height_docked': '720',
    'banjo_renut_dynamic_resolution': 'false',
    'disable_msaa': 'false',
    'renut_shadow_resolution': '"1x"',
    'renut_ambient_occlusion': '"Half"',
    'renut_post_process_precision': '"Game"',
    # The game paces itself (its presentation interval); no extra cap.
    'frame_cap': '"Off"',
    'target_refreshRate': '-1',
    # banjo-nx's PM4 renderer and its keys are not used here.
    'masseffect_renderer_native': 'false',
    'gpu_plugin': '""',
    # N&B's forwarder: Beautiful Katamari's forwarder has its own program id (README.txt, "Four cores").
    'banjo_forwarder_program_id': '"05D6EA1333487100"',
    'banjo_shader_misses': '"sdmc:/switch/katamari-nx/shader_misses"',
}
PLAY = {'banjo_renut_diag_resolve_trace': 'false', 'banjo_renut_diag_gpu_shaders': 'false',
        'banjo_renut_diag_terrain': 'false'}
AB = {'banjo_renut_ab_targets': '"45,48,46,47,50,1"', 'banjo_renut_ab_s': '30', 'banjo_renut_dynamic_resolution': 'false'}
VERIFY = {k: 'true' for k in (
    'banjo_renut_fast_draw_verify', 'banjo_renut_resource_objects_verify', 'banjo_renut_fetch_cache_verify',
    'banjo_renut_whole_buffers_verify', 'banjo_renut_persistent_shared_verify', 'banjo_renut_chain_reuse_verify',
    'banjo_renut_snapshot_deltas_verify', 'banjo_renut_store_cards_verify', 'banjo_renut_record_extents_verify')}
CONFIGS = {
    'renut_verify.toml': (VERIFY, 'every Renut verify mode on (emulator / test runs: slow; the log\'s verify lines '
                                  'must count 0 differences)'),
    'renut_nomsaa.toml': ({'disable_msaa': 'true'}, 'the game\'s MSAA off (one sample per pixel; not pixel-identical)'),
    'renut_648_864.toml': ({'banjo_renut_render_height_handheld': '648', 'banjo_renut_render_height_docked': '864'},
                           'banjo-nx\'s internal resolution: 648 lines handheld, 864 docked (not pixel-identical)'),
    'renut_1080_docked.toml': ({'banjo_renut_render_height_docked': '1080'}, '1080 lines docked (not pixel-identical)'),
    'renut_dynamic.toml': ({'banjo_renut_dynamic_resolution': 'true'},
                           'banjo-nx\'s dynamic resolution, 540-720 handheld / 720-1080 docked (not pixel-identical)'),
}


def apply(text, keys):
    for k, v in keys.items():
        pat = re.compile(r'^(\s*)' + re.escape(k) + r'\s*=.*$', re.M)
        if pat.search(text):
            text = pat.sub(lambda m: f'{m.group(1)}{k} = {v}', text, count=1)
        else:
            text = text.rstrip('\n') + f'\n{k} = {v}\n'
    return text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--banjo', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    base = open(a.banjo, encoding='utf-8').read().replace('sdmc:/switch/banjo-nx/', 'sdmc:/switch/katamari-nx/')
    shipped = apply(base, SHIPPED)
    os.makedirs(os.path.join(a.out, 'configs'), exist_ok=True)
    outs = {
        'renut.toml': (shipped, 'the shipped settings (pixel-identical; diagnostics and logs on)'),
        'renut_play.toml': (apply(shipped, PLAY), 'renut.toml with the render-thread diagnostics off, for playing'),
        'renut_ab.toml': (apply(shipped, AB), 'renut.toml with an in-run A/B list, 30 s halves, target 1 as the '
                                              'control (a Vulkan-only key: nothing on the Switch)'),
    }
    for name, (cfg, what) in CONFIGS.items():
        outs[os.path.join('configs', name)] = (apply(shipped, cfg), what)
    for name, (text, what) in outs.items():
        with open(os.path.join(a.out, name), 'w', encoding='utf-8', newline='\n') as f:
            f.write(HEADER.format(what=what) + text)
        print(name)


if __name__ == '__main__':
    main()
