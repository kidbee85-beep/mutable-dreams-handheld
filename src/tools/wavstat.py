import wave, struct, math, sys
w=wave.open(sys.argv[1]); n=w.getnframes(); s=struct.unpack('<%dh'%(n*2),w.readframes(n)); sec=44100*2
out=[]
for i in range(0,len(s)//sec):
    blk=s[i*sec:(i+1)*sec]; r=math.sqrt(sum(x*x for x in blk)/len(blk))/32768; dc=sum(blk)/len(blk)/32768
    out.append(f"{i}s:{20*math.log10(r+1e-9):.0f}dB/dc{dc:+.3f}")
print(sys.argv[1].split('/')[-1], ' '.join(out[::4]))
