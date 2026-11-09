import os
import json, re, math, sys, numpy as np
R=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","..","firmware","src")+"/"
T=float(sys.argv[2]) if len(sys.argv)>2 else -8.0
js=json.load(open(sys.argv[1]))
files={5:("eng_formant.c","FORMANT_TRIM"),0:("eng_analog.c","ANALOG_TRIM"),1:("eng_digital.c","DIGITAL_TRIM"),2:("eng_phase.c","PHASE_TRIM"),3:("eng_lofi.c","LOFI_TRIM"),
 7:("eng_wheel.c","WHEEL_TRIM"),11:("eng_noise.c","NOISE_TRIM"),15:("eng_juno.c","JUNO_TRIM"),20:("eng_acid.c","ACID_TRIM"),
 23:("eng_strings.c","STRINGS_TRIM"),24:("eng_vps.c","VPS_TRIM"),26:("eng_wseq.c","WSEQ_TRIM"),27:("eng_terrain.c","TERRAIN_TRIM"),
 28:("eng_vector.c","VECTOR_TRIM"),29:("eng_hybrid.c","HY_TRIM")}
tot=0
for e,(fn,arr) in files.items():
    rows=js[str(e)]; L=[np.median([v[1] for v in row]) for row in rows]
    src=open(R+fn).read(); m=re.search(arr+r"\[\]\s*=\s*\{([^}]*)\}",src)
    toks=list(re.finditer(r"-?\d+",m.group(1)))
    if len(toks)!=len(L): print("SKIP",fn,"len",len(toks),"vs",len(L)); continue
    vals=[int(t.group()) for t in toks]; new=[]; n=0
    for v,l in zip(vals,L):
        d=math.ceil((l-T)*2) if l>T else 0
        if d>0: n+=1
        new.append(v-d)
    out=[];pos=0;g=m.group(1)
    for t,nv in zip(toks,new): out.append(g[pos:t.start()]); out.append(str(nv)); pos=t.end()
    out.append(g[pos:]); body="".join(out)
    open(R+fn,"w").write(src[:m.start(1)]+body+src[m.end(1):])
    print(f"{fn:14s} lowered {n:3d} of {len(L)}  (max cut {max(a-b for a,b in zip(vals,new))/2:.1f} dB)"); tot+=n
print("total",tot)
