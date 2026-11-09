import subprocess, numpy as np
import os
S=os.environ.get("MEAS_DIR", os.path.dirname(os.path.abspath(__file__)))
FS=44100.0
def noise():
    r=12345; xs=[]
    for i in range(65536+4096):
        r=(r*1664525+1013904223)&0xFFFFFFFF
        v=(r>>16)-32768; xs.append(int(v/8))   # trunc toward zero like C
    return np.array(xs[4096:],float)
X0=noise()
def resp(model,cut,res):
    r=subprocess.run([S+"/meas","flt",str(model),str(cut),str(res)],capture_output=True)
    y=np.frombuffer(r.stdout,dtype=np.int32).astype(float)
    n=8192; segs=len(y)//n
    Px=np.zeros(n//2+1); Py=np.zeros(n//2+1); Pxy=np.zeros(n//2+1,complex); w=np.hanning(n)
    for k in range(segs):
        a=np.fft.rfft(X0[k*n:(k+1)*n]*w); b=np.fft.rfft(y[k*n:(k+1)*n]*w)
        Px+=abs(a)**2; Pxy+=b*np.conj(a)
    H=np.abs(Pxy/Px); f=np.arange(n//2+1)*FS/n
    return f,20*np.log10(H+1e-12)
def smooth(f,db,pts):
    return [float(np.interp(p,f,db)) for p in pts]
if __name__=="__main__":
    for model,name in ((1,"SVF"),(2,"MOOG ladder")):
        print("==",name)
        for cut in (50,80,105):
            fs,db=resp(model,cut,0)
            # -3 dB point relative to passband
            ref=np.mean(db[(fs>30)&(fs<100)]); i=np.argmax(db< ref-3); fc=fs[i]
            # slope between 2fc and 4fc
            s=(np.interp(4*fc,fs,db)-np.interp(2*fc,fs,db)) if 4*fc<20000 else float('nan')
            print(f" cut {cut:3d}: passband {ref:6.1f} dB, -3 dB at {fc:7.0f} Hz, slope 2fc->4fc {s:6.1f} dB/oct")
            for res in (32,64,96,127):
                f2,d2=resp(model,cut,res); pb=np.mean(d2[(f2>30)&(f2<100)]); pk=d2.max(); fp=f2[np.argmax(d2)]
                print(f"     res {res:3d}: passband {pb:6.1f} dB (loss {ref-pb:5.1f}), peak {pk:6.1f} dB at {fp:7.0f} Hz (+{pk-pb:5.1f} over passband)")
