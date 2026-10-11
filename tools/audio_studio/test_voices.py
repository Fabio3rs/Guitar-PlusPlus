"""Isolated Linux voice/memory comparison, with generated PCM fixtures."""
import json
import pathlib
import subprocess
import sys
import time
from test_cli import fixture, rpc, wait_job


def main():
    exe = str(pathlib.Path(sys.argv[1]).resolve())
    output = pathlib.Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = output / 'tone.wav'
    fixture(source)
    base = [exe, '--mode', 'headless', '--file', str(source), '--set',
            'synth.duration=0.051', '--output-dir', str(output)]
    for steal in [False, True]:
        filename = 'steal.json' if steal else 'reject.json'
        args = ['--compare-voices', '5', '--voice-bursts', '2', '--voice-report', filename]
        if steal:
            args += ['--voice-steal-oldest', '--voice-trim-tail']
        subprocess.run(base + args, stdout=subprocess.DEVNULL, check=True, timeout=30)
        report = json.loads((output / filename).read_text())
        assert len(report['cases']) == 4
        assert len({c['pid'] for c in report['cases']}) == 4
        assert len({c['pcm_fnv1a64'] for c in report['cases']}) == 1
        assert abs(report['pcm_seconds'] - (.051 if steal else .131)) < .0001
        for case in report['cases']:
            n = case['voices']
            assert case['summary']['failed'] == 0, case['summary']
            assert len(case['events']) == n * 4
            assert len(case['warmup_events']) == n * 2
            assert case['summary']['max_active'] <= n
            assert case['summary']['max_active'] >= 1
            assert case['no_sound']
            assert case['memory']['bass_initialized']['rss_bytes'] > 0
            assert case['pcm_capacity_bytes'] >= case['pcm_bytes'] > 0
            if case['strategy'] == 'streams':
                assert case['shared_wav_capacity_bytes'] > 0
            else:
                assert case['shared_wav_capacity_bytes'] == 0
            # Relax exact counts under heavily instrumented/slow machines: API
            # event accounting is invariant even if a voice ends during a burst.
            assert case['summary']['rejected'] == sum(e['error'] == 18 for e in case['events'])
            assert case['summary']['replaced'] == sum(e['replaced'] and e['accepted'] for e in case['events'])
            for start in range(0, len(case['events']), 2 * n):
                burst = case['events'][start:start + 2 * n]
                if burst[-1]['at_ms'] - burst[0]['at_ms'] < report['pcm_seconds'] * 500:
                    assert max(e['active'] for e in burst) == n
                    if steal:
                        assert sum(e['replaced'] for e in burst) == n
                    else:
                        assert sum(not e['accepted'] for e in burst) == n
            if steal:
                assert case['summary']['rejected'] == 0
            else:
                assert case['summary']['replaced'] == 0
    p = subprocess.Popen(base + ['--rpc-stdio'], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, text=True)
    try:
        rpc(p, 'generate')
        assert not wait_job(p)['error']
        assert 'error' in rpc(p, 'voices.compare', {'voices': 33})
        rpc(p, 'voices.load', {'path': str(output / 'reject.json')})
        saved = rpc(p, 'state')['result']['voice_comparison']
        (output / 'bad.json').write_text('{"kind":"voice_comparison","cases":[{}]}')
        assert 'error' in rpc(p, 'voices.load', {'path': str(output / 'bad.json')})
        assert rpc(p, 'state')['result']['voice_comparison'] == saved
        rpc(p, 'voices.compare', {'voices': 5, 'bursts': 50, 'path': 'cancelled.json'})
        time.sleep(.15)
        rpc(p, 'cancel')
        # A following job must complete after the child is reaped; cancellation
        # cannot leave the worker blocked for the full benchmark duration.
        rpc(p, 'generate')
        assert not wait_job(p)['error']
        assert not (output / 'cancelled.json').exists()
        rpc(p, 'shutdown')
        assert p.wait(timeout=10) == 0
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
    assert not list(output.glob('voice-probe-*'))
    print('Voice comparison, memory, report import and cancellation passed')


if __name__ == '__main__':
    main()
