import subprocess, numpy as np, json, sys
import os
S=os.environ.get("MEAS_DIR", os.path.dirname(os.path.abspath(__file__)))
names={0:"ANALOG",1:"DIGITAL",2:"PHASE",3:"LOFI",5:"VOICE",7:"WHEEL",11:"NOISE",12:"FM6",15:"JUNO",16:"OMNI",17:"KALIMBA",19:"GUITAR",20:"ACID",21:"DRIFT",22:"PLAITS",23:"STRINGS",24:"VPS",25:"MODAL",26:"WSEQ",27:"TERRAIN",28:"VECTOR",29:"HYBRID"}
def one(e,p,n):
    o=subprocess.run([S+"/meas_nolim","mix",str(e),str(p),str(n)],capture_output=True,text=True).stdout.split()
    pk,rms,g=float(o[0]),float(o[1]),float(o[2])
    lg=20*np.log10(g)
    return pk-lg if g<0.999 else pk, rms-lg   # pre-limiter peak, rms (dBFS re 32768, mix output)
def run(tag):
    res={}
    for e,nm in names.items():
        npz=int(subprocess.run([S+"/meas","np",str(e)],capture_output=True,text=True).stdout)
        res[e]=[[one(e,p,n) for n in (48,60,72)] for p in range(npz)]
    json.dump(res,open(S+f"/mix_{tag}.json","w")); return res
def summarize(res):
    allr=[]
    for e,nm in names.items():
        r=np.array([[v[1] for v in row] for row in res[e]]); pk=np.array([[v[0] for v in row] for row in res[e]])
        rr=np.median(r,axis=1); pm=pk.max(axis=1)   # a preset's loudness: median over the 3 pitches; peak: worst pitch
        allr+=list(rr)
        print(f"{nm:8s} n={len(rr):3d} rms med {np.median(rr):6.1f} p90 {np.percentile(rr,90):6.1f} max {rr.max():6.1f} | pre-limiter peak max {pm.max():6.1f}, presets over -5.2: {(pm>-5.2).sum():3d}")
    allr=np.array(allr); print("ALL rms median %.1f p75 %.1f p90 %.1f"%(np.median(allr),np.percentile(allr,75),np.percentile(allr,90)))
if __name__=="__main__":
    tag=sys.argv[1]; summarize(run(tag))
