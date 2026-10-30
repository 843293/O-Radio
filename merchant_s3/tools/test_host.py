#!/usr/bin/env python3
"""Compile and run the firmware's host-only regression tests in a temporary directory."""
from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
pure = ['display_idle', 'fm_logic', 'button_click', 'alarm', 'lcd_utf8', 'pcm',
        'sleep_plan', 'alarm_button', 'auto_sleep', 'ui_palette', 'theme_picker']
linked = [('radio', 'radio_core'), ('audio_bus', 'audio_bus'), ('battery', 'battery_monitor')]
cxx = os.environ.get('CXX', 'g++')
with tempfile.TemporaryDirectory(prefix='o-radio-tests-') as folder:
    for name, source in [(name, None) for name in pure] + linked:
        output = str(Path(folder) / name)
        cmd = [cxx, '-std=c++17', '-Wall', '-Wextra', '-I'+str(root/'tests/host'), '-I'+str(root/'include')]
        if source:
            cmd.append(str(root/'src'/f'{source}.cpp'))
        cmd += [str(root/'tests/host'/f'test_{name}.cpp'), '-o', output]
        subprocess.run(cmd, check=True)
        subprocess.run([output], check=True)
        print('PASS:', name, flush=True)
print('PASS: 14 host test executables')
