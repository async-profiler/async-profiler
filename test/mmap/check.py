#!/usr/bin/env python3
"""Exercise the actual asprof -> JFR -> jfrconv pipeline against JNA 5.8.0."""
import os, pathlib, queue, subprocess, sys, threading
root = pathlib.Path(__file__).resolve().parents[2]
java = pathlib.Path(os.environ['JAVA_HOME']) / 'bin/java'
javac = java.with_name('javac')
jna = pathlib.Path(os.environ.get('JNA_JAR', str(root/'test/deps/jna-5.8.0.jar'))).resolve()
if not jna.is_file(): raise SystemExit('Provide JNA_JAR=/path/to/jna-5.8.0.jar (offline is supported)')
chunks = 'chunks' in sys.argv[1:]
cold = 'cold' in sys.argv[1:]
combined = 'combined' in sys.argv[1:]
concurrent = 'concurrent' in sys.argv[1:]
restart = 'restart' in sys.argv[1:]
mode = 'chunks' if chunks else 'concurrent' if concurrent else 'restart' if restart else 'cold' if cold else 'combined' if combined else 'warm'
output = root / 'build/test/mmap' / mode
output.mkdir(parents=True, exist_ok=True)
subprocess.run([str(javac), '--release', '8', '-Xlint:-options', '-cp', str(jna), '-d', str(output),
                str(root/'test/mmap/Demo.java'), str(root/'test/mmap/ConcurrentMappings.java'), str(root/'test/mmap/ChunkMappings.java')], check=True)
p = subprocess.Popen([str(java), '-cp', str(output)+os.pathsep+str(jna), 'probe.ChunkMappings' if chunks else 'probe.ConcurrentMappings' if concurrent else 'probe.Demo'] + (['cold'] if cold else []),
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
q = queue.Queue()
def pump():
    for line in p.stdout:
        print(line, end='', flush=True); q.put(line)
    q.put('EOF')
threading.Thread(target=pump, daemon=True).start()
def until(prefix):
    while True:
        line = q.get(timeout=40)
        if line.startswith(prefix): return line
        if line == 'EOF': raise AssertionError('Demo exited early')
def asprof(*args):
    subprocess.run([str(root/'build/bin/asprof'), *args, str(p.pid)], check=True, timeout=40)
def convert(file, *options):
    subprocess.run([str(java), '-jar', str(root/'build/jar/jfr-converter.jar'), '--mmap',
                    '--include', '.*com.sun.jna.Native.invokeLong.*', *options,
                    str(output/'mappings.jfr'), str(output/file)], check=True, timeout=40)
def total(file):
    lines = (output/file).read_text().splitlines()
    assert any('probe/' in line or 'probe.' in line for line in lines), lines
    return sum(int(line.rsplit(' ', 1)[1]) for line in lines if line.strip())
try:
    until('READY')
    asprof('start', '-e', 'mmap', '-f', str(output/'mappings.jfr'), *(['--nativemem', '0'] if combined else []), *(['--chunktime', '5'] if chunks else []))
    if restart:
        asprof('stop')
        asprof('start', '-e', 'mmap', '-f', str(output/'mappings.jfr'))
    p.stdin.write('go\n'); p.stdin.flush()
    done = until('DONE')
    page = int(dict(x.split('=', 1) for x in done.split()[1:])['bytes'])
    asprof('stop')
    if chunks:
        summary = subprocess.check_output([str(java.with_name('jfr')), 'summary', str(output/'mappings.jfr')], text=True)
        assert any('Chunks:' in line and int(line.split(':')[1]) >= 2 for line in summary.splitlines()), summary
    convert('allocated.collapsed', '--total')
    convert('remaining.collapsed', '--leak', '--tail', '0', '--total')
    convert('remaining-count.collapsed', '--leak', '--tail', '0')
    convert('remaining.html', '--leak', '--tail', '0', '--total')
    assert total('allocated.collapsed') == (page * 3 // 2 if chunks else page * (1201 if concurrent else 6 if cold else 5))
    assert total('remaining.collapsed') == page
    assert total('remaining-count.collapsed') == 1
    assert '<html' in (output/'remaining.html').read_text().lower()
    p.stdin.write('cleanup\n'); p.stdin.flush()
    until('CLEANED')
    assert p.wait(timeout=20) == 0
    print('PASS:', mode, 'asprof attach, JFR, HTML, exact allocated/remaining bytes and allocation count; output:', output)
finally:
    if p.poll() is None: p.kill(); p.wait()
