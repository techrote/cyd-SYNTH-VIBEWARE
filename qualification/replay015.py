"""QA015 offline archive custody and portable replay (no Windows/physical execution)."""
import hashlib, io, pathlib, re, subprocess, sys, tempfile, zipfile

def need(ok, msg):
    if not ok: raise ValueError(msg)

def sha(b): return hashlib.sha256(b).hexdigest()

def members(raw):
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        files, seen = {}, set()
        for i in z.infolist():
            n = i.filename.rstrip('/')
            need(n and all(p not in ('', '.', '..') and not p.endswith((' ', '.')) for p in n.split('/')) and not any(c in n for c in ('\\', ':')), 'unsafe path')
            need(n.casefold() not in seen and (i.external_attr >> 16) & 0o170000 != 0o120000, 'duplicate/symlink')
            seen.add(n.casefold())
            need(i.file_size < 32*1024*1024, 'oversize')
            if not i.is_dir(): files[n] = z.read(i)
        return files

def manifest(f, key, expected):
    seen = set()
    for line in f[key].decode('utf-8-sig').splitlines():
        m = re.fullmatch(r'([a-fA-F0-9]{64})[ \t]+\*?(.+)', line)
        need(m is not None, 'manifest syntax')
        h,n = m.groups(); n = n.replace(chr(92), '/').removeprefix('./')
        need(n in f and n not in seen and sha(f[n]) == h.lower(), 'manifest '+n)
        seen.add(n)
    need(seen == expected, 'manifest coverage')
    return len(seen)

def outer(root, name, digest):
    b = (root/(name+'.zip')).read_bytes(); need(sha(b)==digest, name+' outer digest'); return members(b)

def inner(o, name, digest):
    raw = o[name]; need(sha(raw)==digest, name+' inner digest')
    return members(raw)

def media(f, prefix): return {n[len(prefix):]:b for n,b in f.items() if n.startswith(prefix) and n.endswith(('.wav','.ppm','.csv'))}

G = ('grimoire-m3-windows','4ad949deb61c1d2a24804cde78a79094e2d6d19df6bb7271d42661daa09a88c4','Grimoire-Windows.zip','a2259409784e1337f4bd382be2effe5f267fa32105066831dc0622ddded1b0b5','2cea6483791935d5b0b3e3b0ee80db4eb52917e4','1288e8720b171d8f9687e2ef3d38281ae9093bbd')
P = ('phosphor-m3r-windows','c542f635a303eeba2b8ddd5b4c7e822832fb5414ef11b9f6cdb51b5494da21b2','PHOSPHOR Windows.zip','063f2a1014403c0ef89f0413ab87c09aae7c9b16d897f3a07f0bb41121db9db7','0644c4dd6f23af125e141268f06532ed594d0060','de11403f8bc1f9f8bac2414d1f651a53fc897fd4')

def verify(root, pin):
    n,h,z,ih,head,tree = pin; f=inner(outer(root,n,h),z,ih)
    count=manifest(f,'SHA256SUMS.txt',set(f)-{'SHA256SUMS.txt'})
    need(f['SOURCE.txt'].decode().splitlines()==[head,tree],'source pins')
    need(not any(n.lower().endswith(('.elf','.bin','.hex','.uf2')) for n in f),'firmware payload')
    print(n,count,'manifest checks PASS',flush=True); return f

def replay(f, tests, compiler, flags):
    with tempfile.TemporaryDirectory(prefix='QA 015 source ') as t:
        p=pathlib.Path(t)
        for n,b in f.items():
            if n.endswith(('.hpp','.cpp')): (p/n).write_bytes(b.replace(b'\r\n',b'\n'))
        for src in ['replay',*tests]:
            exe=p/src
            subprocess.run([compiler,'-std=c++17','-pthread',*flags,str(p/(src+'.cpp')),'-o',str(exe)],check=True)
            args=[]
            if src=='replay': (p/'new media').mkdir(); args=[str(p/'new media')]
            subprocess.run([str(exe),*args],cwd=p,check=True,timeout=180)
        actual={x.name:x.read_bytes() for x in (p/'new media').iterdir() if x.suffix in ('.wav','.ppm')}
        need(actual==media(f,'media/'),'media mismatch')
        print(compiler,len(actual),'exact regenerated media PASS',flush=True)

def main(root):
    g,p=verify(root,G),verify(root,P)
    gl=outer(root,'grimoire-m3-linux','2aefbf3595533039c974058f1090f3b533cbfe3aff78378e184a99623cd1ee2b')
    print('Grimoire Linux',manifest({'SHA256SUMS.txt':gl['SHA256SUMS.txt'],**media(gl,'native media/')},'SHA256SUMS.txt',set(media(gl,'native media/'))),'manifest PASS')
    need(media(gl,'native media/')==media(g,'media/'),'Grimoire cross-platform media')
    hl=outer(root,'hybrid-m22-linux','73052960831e945df8a8e26cb081c13c5eedaac209635170bed3ccdc69f6f70d')
    hw=inner(outer(root,'hybrid-m22-windows','4328e52f33852c553bff0c9326fb9082dc9e015a5bcd01b7a61cb632ca7ecb00'),'cluster-native-windows.zip','55c8a1b92a43660137cbb11887aa7b6b2ced83c4431baa73e9d607dfab0148eb')
    print('HYBRID Windows',manifest(hw,'files.sha256',set(hw)-{'files.sha256'}),'manifest PASS')
    print('HYBRID Linux',manifest(hl,'gallery.sha256',{n for n in hl if n.startswith('reaction-gallery/')}),'manifest PASS')
    for key,value in [('source-commit.txt','3a8da3d72abbd31e32fd51f6bf279337d9356a90'),('source-pr-head.txt','8062b096aeb212362d86ace3a4667e3b1ce0e9b0'),('source-tree.txt','04b2437f6753937af876b89fd9b107ad0bd14bef')]:
        need(hl[key].decode().strip()==hw[key].decode().strip()==value,'HYBRID source pin '+key)
    need(media(hl,'reaction-gallery/')==media(hw,'reaction-gallery/'),'HYBRID cross-platform gallery')
    print('HYBRID seven gallery files exact PASS')
    for cc,flags in [('g++',['-O2']),('clang++',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
        replay(g,['strip_test','touch_test'],cc,flags)
        replay(p,['split_test','desktop_test'],cc,flags)

if __name__=='__main__': main(pathlib.Path(sys.argv[1]))
