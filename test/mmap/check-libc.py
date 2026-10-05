#!/usr/bin/env python3
"""Offline FileChannel -> libc -> asprof -> JFR/HTML test. No JNA required."""
import os
import pathlib
import queue
import subprocess
import threading
import sys
import platform

root = pathlib.Path(__file__).resolve().parents[2]
java = pathlib.Path(os.environ['JAVA_HOME']) / 'bin/java'
native = 'native' in sys.argv[1:]
output = root / ('build/test/mmap/native-libc' if native else 'build/test/mmap/libc')
output.mkdir(parents=True, exist_ok=True)
subprocess.run([str(java.with_name('javac')), '-d', str(output),
                str(root / 'test/mmap/NativeMappings.java' if native else root / 'test/mmap/FileMappings.java')], check=True)
native_args = []
if native:
    system = 'darwin' if platform.system() == 'Darwin' else 'linux'
    library = output / ('libmmapfixture.dylib' if system == 'darwin' else 'libmmapfixture.so')
    include = java.parent.parent / 'include'
    subprocess.run(['cc', '-O0', '-g', '-fno-omit-frame-pointer', '-fPIC', '-shared',
                    '-I' + str(include), '-I' + str(include / system), '-pthread',
                    str(root / 'test/mmap/nativeMappings.c'), '-o', str(library)], check=True)
    native_args = [str(library)]
p = subprocess.Popen([str(java), '-cp', str(output),
                      'probe.NativeMappings' if native else 'probe.FileMappings', *native_args],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
lines = queue.Queue()
def pump():
    for line in p.stdout:
        print(line, end='', flush=True)
        lines.put(line)
    lines.put('EOF')
threading.Thread(target=pump, daemon=True).start()
def until(prefix):
    while True:
        line = lines.get(timeout=60)
        if line.startswith(prefix): return line
        if line == 'EOF': raise AssertionError('FileMappings exited early')
def send():
    p.stdin.write('go\n')
    p.stdin.flush()
def asprof(*args):
    subprocess.run([str(root / 'build/bin/asprof'), *args, str(p.pid)], check=True, timeout=60)
def convert(recording, name, leak):
    target = output / name
    subprocess.run([str(java), '-jar', str(root / 'build/jar/jfr-converter.jar'),
                    '--mmap', '--total', '--include', 'native_fixture_map.*' if native else 'Java_sun_nio_ch_.*_map0',
                    *(['--leak', '--tail', '0'] if leak else []), str(recording), str(target)],
                   check=True, timeout=60)
    return target
def total(path):
    return sum(int(line.rsplit(' ', 1)[1]) for line in path.read_text().splitlines() if line.strip())
try:
    until('READY')
    recording = output / 'mappings.jfr'
    start_args = ['start', '-e', 'mmap', '-f', str(recording)]
    if 'combined' in sys.argv[1:]: start_args += ['--nativemem', '0']
    asprof(*start_args)
    if 'restart' in sys.argv[1:]:
        asprof('stop')
        asprof(*start_args)
    send()
    page = int(until('DONE').split('bytes=')[1])
    # Dump keeps the original allocation in this recording; stop after final unmap.
    asprof('dump', '-o', 'jfr')
    expected_pages = 1208 + (platform.system() == 'Linux') if native else 104
    assert total(convert(recording, 'allocated.collapsed', False)) == page * expected_pages
    assert total(convert(recording, 'remaining.collapsed', True)) == page
    convert(recording, 'remaining.html', True)
    send()
    until('CLEANED')
    asprof('stop')
    assert total(convert(recording, 'after-cleanup.collapsed', True)) == 0
    convert(recording, 'after-cleanup.html', True)
    send()
    assert p.wait(timeout=20) == 0
    print('PASS:', 'late JNI/pthreads/partial unmap/MAP_FIXED/errno' if native else 'FileChannel/cross-thread unmap/unaligned offset',
          'JFR and HTML:', output)
finally:
    if p.poll() is None:
        p.kill()
        p.wait()
