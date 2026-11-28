"""Independent Linux reproduction of three exact integrated Windows package source sets."""
import hashlib, io, pathlib, subprocess, sys, tempfile, zipfile
COLLECTION = '7d44a3024a9aa0b695e747acfb2718b6cef731f855756233ad71d584eabc80e0'
PINS = {
 'XENOBIO': ('source/', 'ecda1297486bb1efa32ac814b76bc52e342ab2455d25b0bff9524960fe11e621', {'core.hpp':'6953826440779ef79a53adc76d63b2829079ed88','runtime.hpp':'33d4229f36bf6c8d487dd6cb7c0581248befd981','labeled.hpp':'c35cae93e683db681f5ccd32e34fece8e919d3a0','replay.cpp':'bd315ebd1de6522aaea75fcd44d33e5b0a4c34f6','runtime_test.cpp':'79fef2e5482fef0a97958790c72d12885b4ef013','target_input.hpp':'0242c9305b1dfff902dbca6e73abab198d231d68','target_input_test.cpp':'f5098f32644dd41e6b39d8839ff8454e89926ea7','windows.cpp':'8050853a826f06893acdbe893ff1a64a7ec99857'}),
 'Chronomorph': ('', 'e3268724cacc77f3a22cfc1945a8de2774fbadb88aca82055392c99ac41a1f2c', {'core.hpp':'d55a82e2db096456df496ea12cd41dce5c27c104','replay.cpp':'e5d4d91ec921e5a670c7fff968ee39fcb239ee38','runtime.hpp':'1c6c6bd9fbeb4ca5476b571fcb725d962e50bc97','runtime_test.cpp':'c64356aa2836f4977e1e1f7d47d809c39c9ece41','target_gate.hpp':'eb8e66fa29135e69a69caf1cf713c371c5b5c828','target_gate_test.cpp':'9914c982bc98bc5ab56f5274bffb78627101cb8c','windows.cpp':'4c1ff545883d2d55de3e96e61e1c515d6459a6bd'}),
 'Xenoresonator': ('', '50334ebf59a3ea84224f403af8aa5ee60e3041356416ba28cef766b136d53f59', {'core.hpp':'26eb7ae004f5c5fe69535a7b143848b9fb622305','exchange.hpp':'b69d81eedb2ba27ddae39d69b2cc4cb5e65e0258','replay.cpp':'d8cd008123c59da674d92cbc0e72be877ae6156c','windows.cpp':'0275f435b9e53cd65897b453ff6858679f712faf'})}
def require(ok, msg):
    if not ok: raise RuntimeError(msg)
def sha(b): return hashlib.sha256(b).hexdigest()
def blob(b): return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def main(path):
    raw = pathlib.Path(path).read_bytes()
    require(sha(raw)==COLLECTION, 'collection hash')
    with zipfile.ZipFile(io.BytesIO(raw)) as outer, tempfile.TemporaryDirectory(prefix='qa batch 012 ') as tmp:
        for name,(prefix,digest,pins) in PINS.items():
            data=outer.read('Standalone/'+name+'.zip'); require(sha(data)==digest,name+' archive hash')
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                root=pathlib.Path(tmp)/name; root.mkdir()
                for f,h in pins.items():
                    b=z.read(prefix+f).replace(b'\r\n',b'\n'); require(blob(b)==h,name+'/'+f+' blob'); (root/f).write_bytes(b)
                refs={n[6:]:z.read(n) for n in z.namelist() if n.startswith('media/') and n.endswith(('.wav','.ppm'))}
                prior=None
                for compiler,flags in [('g++',['-O2']),('clang++',['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
                    out=root/compiler; out.mkdir()
                    for f in pins:
                        if not f.endswith('.cpp') or f=='windows.cpp': continue
                        exe=out/(f+'.bin'); subprocess.run([compiler,'-std=c++17','-pthread',*flags,str(root/f),'-o',str(exe)],check=True)
                        args=[str(out)] if f=='replay.cpp' else [str(out/'runtime.ppm')] if name=='XENOBIO' and f=='runtime_test.cpp' else []
                        subprocess.run([str(exe),*args],check=True)
                    media={p.name:p.read_bytes() for p in out.iterdir() if p.suffix in ('.wav','.ppm')}
                    require(media.keys()==refs.keys(),name+' media inventory')
                    diff=[f for f in sorted(refs) if refs[f]!=media[f]]
                    print(name,compiler,'source blobs',len(pins),'Windows media exact',len(refs)-len(diff),'/',len(refs),'differences',diff,flush=True)
                    if prior is not None: require(media==prior,name+' compiler disagreement')
                    prior=media
    print('Native replay and cross-compiler identity PASS; Windows media differences listed separately.',flush=True)
if __name__=='__main__': main(sys.argv[1])
