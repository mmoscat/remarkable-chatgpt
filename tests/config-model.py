import json
import pathlib
import stat
import subprocess
import tempfile

with tempfile.TemporaryDirectory(dir='build-host') as folder:
    path = pathlib.Path(folder) / 'config.json'
    original = {'api_key': 'fixture-only', 'model': 'old-model', 'timezone': 'America/New_York', 'demo_mode': False, 'custom': {'keep': True}}
    path.write_text(json.dumps(original))
    def run(model):
        return subprocess.run(['build-host/config-model', model, str(path)], capture_output=True)
    assert run('claude-haiku-5-5').returncode == 0
    assert json.loads(path.read_text()) == dict(original, model='claude-haiku-5-5')
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    previous = path.read_bytes()
    assert run('bad model').returncode != 0 and path.read_bytes() == previous
    for invalid in ['{bad json', '{}']:
        path.write_text(invalid)
        assert run('claude-haiku-5-5').returncode != 0 and path.read_text() == invalid
print('PASS: model updates preserve key/settings, owner-only permissions, invalid input leaves config unchanged')
