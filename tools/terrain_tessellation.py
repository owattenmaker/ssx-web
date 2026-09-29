"""Conservative bicubic triangle approximation error in source-native metres."""
import math

def control_net(coefficients):
 c=coefficients
 return [[[sum(c[v*4+u][k]*math.comb(i,u)/math.comb(3,u)*math.comb(j,v)/math.comb(3,v) for v in range(j+1)for u in range(i+1))for k in range(3)]for i in range(4)]for j in range(4)]

def error_constant(coefficients):
 b=control_net(coefficients)
 uu=max(math.sqrt(sum((6*(b[j][i+2][k]-2*b[j][i+1][k]+b[j][i][k]))**2 for k in range(3)))for j in range(4)for i in range(2))
 vv=max(math.sqrt(sum((6*(b[j+2][i][k]-2*b[j+1][i][k]+b[j][i][k]))**2 for k in range(3)))for j in range(2)for i in range(4))
 uv=max(math.sqrt(sum((9*(b[j+1][i+1][k]-b[j+1][i][k]-b[j][i+1][k]+b[j][i][k]))**2 for k in range(3)))for j in range(3)for i in range(3))
 # Triangle barycentric Taylor remainder: coordinate variances <=h²/4 and
 # mixed absolute covariance <=h²/4. Vector error <=constant*h².
 return (uu+vv)/8+uv/4

def resolution(coefficients,tolerance=.02,minimum=8,maximum=64):
 if tolerance<=0 or minimum<1 or maximum<minimum:raise ValueError('Invalid tessellation limits')
 constant=error_constant(coefficients);n=minimum
 while n<maximum and constant/n**2>tolerance:n=min(n*2,maximum)
 return n,constant/n**2
