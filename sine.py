from math import sin, pi 
import matplotlib.pyplot as plt

ADC_RESOLUTION = 2**8
REF = 1 #V
LSB = REF/ADC_RESOLUTION
A = 5 #V
OFFSET = 1.75 #V
attenuator = lambda v: (v*8.25e3)/(1000+499e3+487e3+8.25e3)
quantize =  lambda v: int(v/LSB) 

N = 50
nn = [n for n in range(0, N)]
vv = [quantize(attenuator(A*sin((2*pi*n)/N)) + OFFSET) for n in nn] 

plt.stem(nn, vv)   
plt.show()
