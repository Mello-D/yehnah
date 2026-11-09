import subprocess, numpy as np, sys, json
import os
S=os.environ.get("MEAS_DIR", os.path.dirname(os.path.abspath(__file__)))
FS=44100.0
def render(e,p,n):
    r=subprocess.run([S+"/meas","osc",str(e),str(p),str(n)],capture_output=True)
    return np.frombuffer(r.stdout,dtype=np.int32).astype(np.float64)
def analyse(x,n):
    if len(x)<40000 or np.max(np.abs(x))<64: return None
    t=np.arange(len(x)); x=x-np.polyval(np.polyfit(t,x,3),t); N=len(x); a=np.arange(N)*2*np.pi/N
    w=0.35875-0.48829*np.cos(a)+0.14128*np.cos(2*a)-0.01168*np.cos(3*a); X=np.abs(np.fft.rfft(x*w))**2; X[:int(40/(FS/N))]=0
    nom=440*2**((n-69)/12); df=FS/len(x)
    best=None
    for oc in (-2,-1,0,1,2):
        f0n=nom*2.0**oc
        if f0n<30 or f0n>6000: continue
        for c in np.linspace(f0n*0.985,f0n*1.015,61):
            ks=np.arange(1,int(21000/c)+1); idx=np.round(ks*c/df).astype(int)
            sc=sum(X[max(0,i-5):i+6].sum() for i in idx)
            if best is None or sc>best[1]: best=(c,sc)
    if best is None: return None
    f0=best[0]; mask=np.zeros(len(X),bool); mask[:int(40/df)]=True
    for k in range(1,int(22000/f0)+1):
        i=int(round(k*f0/df)); mask[max(0,i-5):i+6]=True
    tot=X.sum(); out=X[~mask].sum()
    mx=X.max(); worst=X[~mask].max() if (~mask).any() else 0
    return (10*np.log10(out/tot+1e-30), 10*np.log10(worst/mx+1e-30))
if __name__=="__main__":
    engs=[int(a) for a in sys.argv[1].split(",")]; notes=[int(a) for a in sys.argv[2].split(",")]; cap=int(sys.argv[3])
    res={}
    for e in engs:
        npz=int(subprocess.run([S+"/meas","np",str(e)],capture_output=True,text=True).stdout)
        ps=sorted(set(np.linspace(0,npz-1,min(cap,npz)).astype(int)))
        for n in notes:
            rows=[]
            for p in ps:
                a=analyse(render(e,p,n),n)
                if a: rows.append((int(p),float(a[0]),float(a[1])))
            res[f"{e}:{n}"]=rows
    json.dump(res,open(S+"/alias_%s.json"%sys.argv[4],"w"))
    for k,rows in res.items():
        if not rows: print(k,"none"); continue
        o=np.array([r[1] for r in rows]); w=np.array([r[2] for r in rows])
        print(f"eng {k:7s} n={len(rows):3d}  nonharm energy dB: med {np.median(o):6.1f} worst {o.max():6.1f} | worst spur re peak dB: med {np.median(w):6.1f} worst {w.max():6.1f}")
