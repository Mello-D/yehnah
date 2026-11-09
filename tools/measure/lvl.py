import subprocess, numpy as np, alias, json, sys
S=alias.S
names={0:"ANALOG",1:"DIGITAL",2:"PHASE",3:"LOFI",5:"VOICE",7:"WHEEL",11:"NOISE",12:"FM6",15:"JUNO",16:"OMNI",17:"KALIMBA",19:"GUITAR",20:"ACID",21:"DRIFT",22:"PLAITS",23:"STRINGS",24:"VPS",25:"MODAL",26:"WSEQ",27:"TERRAIN",28:"VECTOR",29:"HYBRID"}
def measure(e,p,note=60):
    x=alias.render(e,p,note)
    if len(x)<40000: return None
    td=float(subprocess.run([S+"/meas","trimdb",str(e),str(p)],capture_output=True,text=True).stdout.split()[2])
    # use the loudest 0.25 s window (decaying sounds count by their body), DC removed
    y=x-x.mean(); w=11025; best=0
    for k in range(0,len(y)-w,2205): best=max(best,np.sqrt(np.mean(y[k:k+w]**2)))
    pk=np.abs(y).max()
    return (20*np.log10(best/32768+1e-9)+td, 20*np.log10(pk/32768+1e-9)+td, td)
if __name__=="__main__":
    res={}
    for e,nm in names.items():
        n=int(subprocess.run([S+"/meas","np",str(e)],capture_output=True,text=True).stdout)
        rows=[]
        for p in range(n):
            m=measure(e,p)
            rows.append(None if m is None else [float(v) for v in m])
        res[e]=rows
        r=np.array([m[0] for m in rows if m]); pk=np.array([m[1] for m in rows if m])
        if len(r)==0: print(nm,"none"); continue
        print(f"{nm:8s} n={len(r):3d} loud-window rms median {np.median(r):6.1f}  p10 {np.percentile(r,10):6.1f} p90 {np.percentile(r,90):6.1f} max {r.max():6.1f} | peak max {pk.max():6.1f}")
    json.dump(res,open(S+"/lvl_before.json","w"))
