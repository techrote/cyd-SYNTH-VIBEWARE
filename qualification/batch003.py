"""Pinned batch 003 desktop-only assembly and Windows replay.
assemble: python batch003.py assemble DOWNLOADS COLLECTION002.zip OUTPUT.zip
replay: python batch003.py replay INNER_ARCHIVES NEW_OUTPUT_DIRECTORY
No firmware; no physical or redistribution approval implied.
"""
import json, os, re, subprocess, sys, tempfile, zipfile
from pathlib import Path
from assemble import checked_zip, need, sha

# title, run, artifact, author head, checkout, tree, outer digest, inner name, inner digest, app, test, receipt
I = '724d4d2251d9f190e1c412432594239ccd8325fd'
C = '61e8fe9fe42f1fe82cec8a7f137c6af80e7c894e'
T = '3a283affa72cff41b00c8a2d6a1ccd72b2f5394f'
PINS = [
 ('PHASESPACE M4',46108123157,15248387458,I,C,T,'72a6deb2ad1d97f87239370b3e19bbf79422ebcdd2632358a73abc6fdf37bb22','phasespace-windows.zip','d1049fa7fa6266cb9a76419eafb32ac6bf9124c491dd91b1879cc68da611127d','phasespace.exe','phasespace_replay.exe',None),
 ('Xenoresonator M4',46108123155,15248417526,I,C,T,'ee07cf82df97a31d1f2e63ec180c0efc0c9c3f6f87eaffd62b6204f8d03246fb','Xenoresonator.zip','b0cdbd473c36e60c06d3ad9c7965b9f5b622b44504ce5d093ca3a155dc5cc634','Xenoresonator.exe','replay.exe','SHA256SUMS.txt'),
 ('CHRONOGRID M1',46108951057,15248384121,'153e6bbd563242c9bc8bbeda47d1e40158877d41','153e6bbd563242c9bc8bbeda47d1e40158877d41','4127541750c6ea32150ee33bdac9369aa2a054f9','c7d55681e0cdc137de26434910442c8d38466e894f7929ee0cee3710a6c7c0c4','chronogrid-windows.zip','faee421ca6afb6c30bae9f12aaFEa00e1e50d6daaa66a78ce0fb98f7848e4acf'.lower(),'chronogrid.exe','chronogrid_replay.exe','manifest.sha256'),
 ('PHASESPACE M4 accelerated P2',46106372662,15247846558,'772a678a3963c6200a9afbcd59806618999dfa3c','376ff23b33f8a2e834ffeabb0b98446a59d15f6d','fcc12193acef5a385618b330a86e805a9a864569','09acc3f394dfb16649034f3309542a81e59b649582382bd2b37c62fb360054f9','platform-strip-windows.zip','90944795c30721e3b80b6da67b141316bbe932cc8e48e45c4edac27b9ab84b20','phasespace.exe','phasespace_replay.exe',None),
]
OLD = 'cf975897c86f933e50880f66c866a6b8d59c0e3ba0477cfeeb744f9653b36538'
CLUSTER = '9cf94d9e342db625cccf1506c59ff1754da16921dfbc74f8ed5a66e8ea6d2a98'

def audit(raw, pin, pe=False):
    name,run,aid,head,checkout,tree,outer,archive,digest,app,test,receipt = pin
    need(sha(raw)==digest, 'inner digest: '+name)
    with checked_zip(raw) as z:
        files = {n for n in z.namelist() if not n.endswith('/')}
        need(all(Path(n).suffix.lower() in {'.exe','.wav','.ppm','.md','.txt','.hpp','.cpp','.sha256'} for n in files), 'unexpected payload')
        need({app,test,'README.md'} <= files, 'missing entry')
        if aid==15247846558:
            need(z.read('platform-source.txt').decode().strip()==checkout, 'platform source mismatch')
            need(z.read('consumer-base.txt').decode().strip()=='072ede0c2909836f90639d2112ea3786d770795d', 'consumer mismatch')
        else:
            need(z.read('source-commit.txt').decode().strip()==checkout and z.read('source-tree.txt').decode().strip()==tree, 'source mismatch')
        seen=set()
        if receipt:
            for line in z.read(receipt).decode('utf-8-sig').splitlines():
                m=re.fullmatch(r'([0-9a-fA-F]{64}) (?:[ *])?(.+)',line)
                need(m is not None,'invalid receipt'); h,p=m.groups()
                need(p in files and p not in seen and sha(z.read(p))==h.lower(),'receipt mismatch: '+p); seen.add(p)
            need(seen==files-{receipt},'incomplete receipt')
        exes={n for n in files if n.lower().endswith('.exe')}
        need(exes==({app,test,'platform_strip_test.exe','platform_topology_test.exe'} if aid==15247846558 else {app,test}),'unexpected executable set')
        imports={}
        if pe:
            for n in sorted(exes):
                with tempfile.TemporaryDirectory(prefix='QA PE ') as d:
                    p=Path(d)/'check.exe'; p.write_bytes(z.read(n))
                    out=subprocess.check_output(['objdump','-p',str(p)],text=True,timeout=15)
                need('file format pei-x86-64' in out,'wrong PE architecture')
                dll=sorted(set(re.findall(r'DLL Name:\s+(\S+)',out,re.I)))
                need(dll and all(x.lower() in {'kernel32.dll','user32.dll','gdi32.dll','winmm.dll','ws2_32.dll'} or re.fullmatch(r'api-ms-win-crt-[a-z]+-l1-1-0\.dll',x.lower()) for x in dll),'unexpected imports: '+str(dll))
                imports[n]=dll
    return dict(name=name,run=run,artifact=aid,source=head,checkout=checkout,tree=tree,outer_sha256=outer,inner_sha256=digest,executable=app,receipt_entries=len(seen),imports=imports,independent_windows='NOT RUN',physical='NOT RUN',rights='HOLD')

def outer(root,pin):
    raw=(root/f'github-actions-artifact-{pin[2]}.zip').read_bytes()
    need(sha(raw)==pin[6],'outer digest')
    with checked_zip(raw) as z:
        n=pin[7]; r=n[:-4]+'.sha256'
        need(set(z.namelist())=={n,r},'outer members')
        need(z.read(r).decode().strip().lower()==pin[8],'inner receipt')
        return z.read(n)

def assemble(root,old,output):
    verified=[]
    for p in PINS:
        raw=outer(root,p); m=audit(raw,p,True); verified.append((p[7],raw,m))
    prior=old.read_bytes(); need(sha(prior)==OLD,'batch 002 digest')
    with checked_zip(prior) as z:
        raw=z.read('cluster-native-windows.zip'); need(sha(raw)==CLUSTER,'retained cluster digest')
        m=next(x for x in json.loads(z.read('manifest.json')) if x['inner_sha256']==CLUSTER)
        m['independent_windows']='PASS: 46104942985 exact extracted replay and clean M10 source CTest'
        verified.append(('cluster-native-windows.zip',raw,m))
    # P2 is verified, but not duplicated in the collection: same instrument, optional adapter.
    with zipfile.ZipFile(output,'x',compression=zipfile.ZIP_STORED) as z:
        def put(n,b): z.writestr(zipfile.ZipInfo(n,(2026,11,28,0,0,0)),b)
        for n,raw,m in verified:
            if n!='platform-strip-windows.zip': put(n,raw)
        put('manifest.json',json.dumps([m for n,raw,m in verified if n!='platform-strip-windows.zip'],indent=2)+'\n')
        put('optional-platform-audit.json',json.dumps(verified[3][2],indent=2)+'\n')
        put('README.txt','CYD VIBEWARE Collection 003 — Draft/HOLD\nExtract each instrument ZIP separately. Start at low volume. Read each README.\nPHASESPACE: phasespace.exe; M drag topology, R gesture loop, X panic.\nXenoresonator: Xenoresonator.exe; Strike/Rake, material and Couple controls, Panic.\nCHRONOGRID: chronogrid.exe; paint steps, select scenes, tempo, mute and transport.\nVECTORFIELD: cluster_multi_field.exe; local first, M mute, Esc exit.\nNo external MSVC runtime imports identified. UCRT is a Windows system component.\nNew M4/M1 independent Windows replay pending; retained M10 replay PASS.\nNo physical qualification, firmware, or redistribution approval.\n')
    with checked_zip(output.read_bytes()) as z:
        for n,raw,m in verified:
            if n!='platform-strip-windows.zip': need(z.read(n)==raw,'altered package')
    print(json.dumps([m for _,_,m in verified],indent=2)); print('SHA256',sha(output.read_bytes()))

def replay(source,output):
    need(os.name=='nt','Windows execution required')
    for pin in PINS:
        raw=(source/pin[7]).read_bytes(); audit(raw,pin)
        root=output/pin[0]; root.mkdir(parents=True,exist_ok=False)
        with checked_zip(raw) as z:
            z.extractall(root)
            for n in z.namelist():
                if not n.endswith('/'): need(sha((root/n).read_bytes())==sha(z.read(n)),'extraction mismatch')
        def run(*args):
            print('RUN',args,flush=True); subprocess.run([str(a) for a in args],cwd=root,check=True,timeout=90)
        media=root/'QA regenerated media'; media.mkdir()
        run(root/pin[10],media)
        expected=(output/PINS[0][0]/'media') if pin[2]==15247846558 else root/'media'
        names={p.name for p in expected.iterdir() if p.is_file()}
        need(names=={p.name for p in media.iterdir() if p.is_file()},'media set mismatch')
        for n in names: need(sha((media/n).read_bytes())==sha((expected/n).read_bytes()),'media mismatch: '+n)
        run(root/pin[9],'--smoke')
        if pin[2]==15247846558:
            run(root/'platform_strip_test.exe'); run(root/'platform_topology_test.exe')
        print('PASS extracted replay',pin[0],len(names),flush=True)

if __name__=='__main__':
    if sys.argv[1]=='assemble': assemble(*(Path(x).resolve() for x in sys.argv[2:]))
    elif sys.argv[1]=='replay': replay(*(Path(x).resolve() for x in sys.argv[2:]))
    else: raise ValueError('expected assemble or replay')
