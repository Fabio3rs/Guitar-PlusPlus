"""Adaptive cache CLI/RPC chain using a generated, redistributable tone."""
import json
import pathlib
import subprocess
import sys

from test_cli import fixture, rpc, wait_job


def main():
    exe = str(pathlib.Path(sys.argv[1]).resolve())
    output = pathlib.Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = output / 'source.wav'
    fixture(source)
    base = [exe, '--mode', 'headless', '--file', str(source), '--output-dir', str(output)]
    run = subprocess.run(base + ['--whole-song', '--cache-generate', '--cache-prepare',
                                '--cache-save', 'bank.gppfc', '--analysis-save', 'analysis.json',
                                '--song-report', 'song.json', '--cache-export', 'sample', '--json'],
                         capture_output=True, text=True, check=True, timeout=30)
    state = json.loads(run.stdout)['adaptive']
    assert state['regions'] == 1 and state['samples'] == 5 and state['current'] and state['prepared'], state
    assert state['file_bytes'] == (output / 'bank.gppfc').stat().st_size
    assert len(list(output.glob('sample-*.wav'))) == 5
    preserved = (output / 'bank.gppfc').read_bytes()
    regenerated = subprocess.run(base + ['--cache-load', str(output / 'bank.gppfc'),
                                         '--cache-generate', '--set', 'fx.drive=2', '--json'],
                                 capture_output=True, text=True, check=True, timeout=30)
    assert json.loads(regenerated.stdout)['adaptive']['settings']['fx.drive'] == 2
    rescanned = subprocess.run(base + ['--cache-load', str(output / 'bank.gppfc'),
                                       '--whole-song', '--set', 'song.hop=0.2',
                                       '--song-report', 'rescan.json', '--json'],
                               capture_output=True, text=True, check=True, timeout=30)
    assert json.loads(rescanned.stdout)['adaptive']['observations'] == 5
    rescan = json.loads((output / 'rescan.json').read_text())['cache']
    assert rescan['song']['settings']['song.hop'] == .2 and not rescan['sets']
    p = subprocess.Popen(base + ['--rpc-stdio'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True)
    try:
        def job(method, params=None):
            result = rpc(p, method, params)
            assert 'result' in result and result['result']['job_id'] > 0, result
            state = wait_job(p)
            assert not state['error'], state
            return state

        # A bank alone retains enough context to regenerate FX without scanning.
        job('cache.load', {'path': str(output / 'bank.gppfc')})
        rpc(p, 'cache.set', {'fx.drive': 2})
        regenerated = job('cache.generate')
        assert regenerated['adaptive']['current']
        job('song.load', {'path': str(output / 'analysis.json')})
        job('cache.load', {'path': str(output / 'bank.gppfc')})
        job('cache.prepare', {'voices': 5})
        assert 'result' in rpc(p, 'cache.preview', {'mask': 31, 'time': 0})
        regions = rpc(p, 'song.regions', {'offset': 0, 'limit': 1})['result']['region_list']
        assert len(regions) == 1 and regions[0]['start'] == 0 and regions[0]['end'] == 1
        assert 'error' in rpc(p, 'song.regions', {'offset': -1})
        assert 'error' in rpc(p, 'cache.set', {'song.fft': 123})
        job('cache.compare', {'mask': 5})
        assert 'result' in rpc(p, 'cache.preview', {'mode': 'direct', 'mask': 5})
        assert 'result' in rpc(p, 'cache.preview', {'mode': 'individual', 'mask': 5})
        assert 'error' in rpc(p, 'cache.preview', {'mode': 'direct', 'mask': 3})
        job('cache.sustain')
        assert 'result' in rpc(p, 'cache.preview', {'mode': 'sustain', 'mask': 1})
        assert 'result' in rpc(p, 'cache.release', {'fret': 0, 'fade_ms': 20})
        rpc(p, 'cache.set', {'fx.drive': 2})
        assert not rpc(p, 'cache.stats')['result']['current']
        assert 'error' in rpc(p, 'cache.prepare')
        job('cache.generate')
        job('cache.prepare')
        rpc(p, 'transport', {'action': 'play'})
        rpc(p, 'cache.simulate', {'interval': .05, 'mask': 3})
        rpc(p, 'transport', {'action': 'pause'})
        assert not rpc(p, 'cache.stats')['result']['simulating']
        job('cache.load', {'path': str(output / 'bank.gppfc')})
        (output / 'broken.gppfc').write_bytes(preserved[:30])
        rpc(p, 'cache.load', {'path': str(output / 'broken.gppfc')})
        failed = wait_job(p)
        assert failed['error'] and failed['adaptive']['current']
        job('cache.benchmark', {'iterations': 20, 'voices': 5, 'path': 'profiles.json'})
        report = json.loads((output / 'profiles.json').read_text())
        assert len(report['cases']) == 7
        assert len({c['playback']['pid'] for c in report['cases']}) == 7
        assert report['cases'][-1]['sets'] == 1
        for case in report['cases']:
            assert pathlib.Path(case['bank_path']).is_file()
            assert len(case['playback']['events']) == 20
            assert case['playback']['failures'] == 0
            assert case['playback']['memory']['prepared']['rss_bytes'] > 0
            assert case['trigger_api']['p50_ms'] >= 0
        assert (output / 'bank.gppfc').read_bytes() == preserved
        rpc(p, 'shutdown')
        assert p.wait(timeout=10) == 0
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
    print('Adaptive CLI/RPC tests passed')


if __name__ == '__main__':
    main()
