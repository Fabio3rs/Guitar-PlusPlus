"""End-to-end offline client: no sound hardware, no copyrighted fixtures."""
import json
import math
import pathlib
import select
import struct
import subprocess
import sys
import time
import wave


def fixture(path):
    with wave.open(str(path), 'wb') as out:
        out.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
        out.writeframes(b''.join(struct.pack('<hh', *(int(12000 * math.sin(2 * math.pi * 440 * i / 44100)),) * 2)
                                for i in range(44100)))


def rpc(process, method, params=None):
    process.stdin.write(json.dumps(dict(jsonrpc='2.0', id=method, method=method, params=params or {})) + '\n')
    process.stdin.flush()
    assert select.select([process.stdout], [], [], 20)[0], f'RPC timeout: {method}'
    response = json.loads(process.stdout.readline())
    assert response['id'] == method
    return response


def wait_job(process):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        state = rpc(process, 'state')['result']
        if not state['busy']:
            return state
        time.sleep(.015)
    raise AssertionError('Job timeout')


def main():
    exe = str(pathlib.Path(sys.argv[1]).resolve())
    output = pathlib.Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = output / 'source.wav'
    fixture(source)
    base = [exe, '--mode', 'headless', '--output-dir', str(output)]
    run = subprocess.run(base + ['--file', str(source), '--set', 'synth.duration=0.051', '--export', 'fail.wav', '--json'],
                         text=True, capture_output=True, timeout=30, check=True)
    state = json.loads(run.stdout)
    assert state['device'] == 0 and state['audible_latency_ms'] is None
    assert any(n['midi'] == 69 for n in state['notes'])
    assert abs(state['processed_seconds'] - .131) < .0001
    with wave.open(str(output / 'fail.wav')) as wav:
        assert wav.getnchannels() == 2 and wav.getsampwidth() == 2
    bad = subprocess.run(base + ['--set', 'synth.fretMask=32'], capture_output=True, timeout=10)
    assert bad.returncode != 0
    bench = subprocess.run(base + ['--file', str(source), '--benchmark', '2'], capture_output=True, text=True, check=True, timeout=30)
    assert json.loads(bench.stdout)['iterations'] == 2
    report = json.loads((output / 'benchmark.json').read_text())
    assert report['no_sound'] and report['audible_latency_ms'] is None
    assert report['pending']['command_to_audio_system']
    for metric in report['metrics'].values():
        samples = sorted(metric['samples_ms'])
        assert len(samples) == metric['count'] == 2
        assert 0 <= metric['first_ms']
        assert metric['p50_ms'] == samples[0]
        assert metric['p95_ms'] == metric['p99_ms'] == metric['max_ms'] == samples[-1]
        assert abs(metric['mean_ms'] - sum(samples) / 2) < 1e-8
    with open(output / 'rpc-stderr.log', 'w') as error:
        p = subprocess.Popen(base + ['--rpc-stdio'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=error, text=True)
        try:
            assert 'result' in rpc(p, 'load', {'path': str(source)})
            assert not wait_job(p)['error']
            rpc(p, 'seek', {'time': .2})
            rpc(p, 'set', {'synth.loop': True, 'synth.loopBody': .1})
            rpc(p, 'generate')
            state = wait_job(p)
            assert not state['error'] and state['loop_end'] > state['loop_begin']
            rpc(p, 'slot.store', {'slot': 0})
            assert 'error' in rpc(p, 'benchmark', {'iterations': 0})
            assert 'error' in rpc(p, 'benchmark', {'path': '../escape.json'})
            rpc(p, 'benchmark', {'iterations': 2, 'path': 'rpc-benchmark.json'})
            completed = wait_job(p)
            assert not completed['error'] and completed['benchmark']['iterations'] == 2
            assert completed['processed_seconds'] == state['processed_seconds']
            assert (output / 'rpc-benchmark.json').exists()
            rpc(p, 'preset.save', {'path': 'loop.json'})
            rpc(p, 'export', {'path': 'loop.wav', 'float32': True})
            assert not wait_job(p)['error']
            assert b'smpl' in (output / 'loop.wav').read_bytes()
            for mode in ['original', 'raw', 'live', 'processed']:
                response = rpc(p, 'preview', {'mode': mode})
                assert 'error' not in response, response
            rpc(p, 'preview', {'mode': 'live'})
            settings_before = rpc(p, 'state')['result']['settings']
            assert 'error' in rpc(p, 'set', {'fx.pre.frequency': 30000})
            assert rpc(p, 'state')['result']['settings'] == settings_before
            rpc(p, 'transport', {'target': 'preview', 'action': 'stop'})
            assert 'error' in rpc(p, 'set', {'fx.drive': 100})
            assert 'error' in rpc(p, 'export', {'path': '../escape.wav'})
            rpc(p, 'load', {'path': str(output / 'missing.ogg')})
            failed = wait_job(p)
            assert failed['error'] and failed['source'] == str(source)
            rpc(p, 'selection', {'start': .1, 'end': .3})
            rpc(p, 'transport', {'action': 'loop', 'start': .2, 'end': .4})
            loop_state = rpc(p, 'state')['result']
            assert loop_state['selection_start'] == .2 and loop_state['selection_end'] == .4
            rpc(p, 'selection', {'start': .1, 'end': .3})
            rpc(p, 'export', {'path': 'segment.wav', 'segment': True, 'raw': True})
            assert not wait_job(p)['error']
            with wave.open(str(output / 'segment.wav')) as wav:
                assert abs(wav.getnframes() / wav.getframerate() - .2) < .0001
            rpc(p, 'shutdown')
            assert p.wait(timeout=10) == 0
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
    (output / 'report.json').write_text(json.dumps({'cli': 'passed', 'rpc': 'passed', 'exports': 'passed'}, indent=2))
    print('Audio CLI/RPC/export tests passed')


if __name__ == '__main__':
    main()
