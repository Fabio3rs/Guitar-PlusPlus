"""Exercise the real CMake install rules against an isolated authored library."""
import pathlib
import subprocess
import sys
import tempfile

exe, cmake, rules = [str(pathlib.Path(p).resolve()) for p in sys.argv[1:4]]


def run(*args, ok=True):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=30)
    assert (result.returncode == 0) == ok, (result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix='gpp-sfx-package-') as tmp:
    root = pathlib.Path(tmp)
    specs, generated = root / 'specs', root / 'generated'
    specs.mkdir()
    recipe = specs / 'click.lua'
    template = 'return {schemaVersion=1,id="%s",duration=.01,layers={{type="noise"}}}'
    recipe.write_text(template % 'click')
    (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.16)
project(SfxInstallTest NONE)
add_executable(gpp_sfxgen IMPORTED GLOBAL)
set_target_properties(gpp_sfxgen PROPERTIES IMPORTED_LOCATION "{pathlib.Path(exe).as_posix()}")
set(GPP_PACKAGE_RUNTIME ON)
include("{pathlib.Path(rules).as_posix()}")
gpp_install_sfx_library("${{CMAKE_CURRENT_SOURCE_DIR}}/specs" "${{CMAKE_CURRENT_SOURCE_DIR}}/generated")
''')
    build = root / 'build'
    run(cmake, '-S', root, '-B', build)

    def install(component, destination, ok=True):
        return run(cmake, '--install', build, '--prefix', root / destination,
                   '--component', component, ok=ok)

    install('SfxAssets', 'missing', ok=False)
    install('Runtime', 'missing-runtime', ok=False)
    run(exe, '--all', specs, '--output-dir', generated, '--require-filename-id')
    (generated / 'unrelated.wav').write_bytes(b'do not package')
    install('SfxAssets', 'valid')
    assert (root / 'valid/data/sounds/specs/click.lua').exists()
    assert [p.name for p in (root / 'valid/data/sounds/generated').iterdir()] == ['click.wav']
    install('Runtime', 'runtime')
    assert (root / 'runtime/data/sounds/generated/click.wav').exists()

    # Editing an ID after configure must fail even if the old WAV still exists.
    recipe.write_text(template % 'renamed')
    failure = install('SfxAssets', 'mismatch', ok=False)
    assert 'filename stem' in failure.stderr
    assert not (root / 'mismatch/data/sounds/generated/click.wav').exists()
    install('Runtime', 'mismatch-runtime', ok=False)
print('SFX packaging: required WAVs, filename/ID validation and authored allowlist passed')
