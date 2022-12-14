#define _CRT_SECURE_NO_DEPRECATE
#define _src(y,x,w) src[w*y+x]
#define _grad(y,x,a,w) grad[a*w*y+a*x]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void anIsoDiffusion(float* dst ,float* src, int w ,int h, float dt, float threshold, int nb_iter, int nb_smoothings, float a1, float a2);
void isoDiffusion(float* dst ,float* src, int w ,int h, float lambda, float sigma, int iter);

int main()
{
	char* fileName = "degraded.raw";
	char* saveName = "experiment7.raw";
    int width   = 1216;
	int height  = 1220;
	int size = width * height;
	float* data = new float[width*height];
    float* dst = new float[width*height];


	//aniso
	int iter = 10;              //Number of iterations - Maximum number of complete iterations, default value is 20.
	int smoothings = 1;         //Smoothings per iteration - Number of smoothings by anisotropic Gaussian with 3x3 mask per iterations
	float a1 = 0.5f;
	// a1 (Diffusion limiter along minimal variations) - a1 influences the shape of the smoothing mask. f1(l1,l2)= (1.0+l1+l2)^-a1. The smoothing in each iteration is defined by a tensor (2x2 matrix), that is linear combination of tensors corresponding to minimal and maximal eigenvalue of structure tensor. f1 and f2 are weights of the tensors (see Tschumperle and Deriche, 2005). 

	float a2 = 1.2f;
    // a2 (Diffusion limiter along maximal variations) -. a2 influences the shape of the smoothing mask. f2(l1,l2)= (1.0+l1+l2)^-a2. 

	float dt = 20;
	//dt (Time step) - Default value is 20. The result of the filter is proportional to the step, but too long time step yields numerical instability of the procedure. 

	float threshold = 25;
	//Edge threshold height - Defines minimum "strength" of edges that will be preserved by the filter. Default value is 5. 
	
	// this is about isotropic.
	int iter2 = 20;
	float lambda = 0.25;
	float sigma = 20;



	// file open , fileName
	FILE* pLoad = fopen(fileName,"rb");
	
	if(pLoad == NULL)
	{
		printf("Load file open error\n");
        return false;
	}
	
	// file read  , data , size
	fread(data,sizeof(float),size,pLoad);
	
	if(fclose(pLoad)!=0)
	{
		printf("Load file close error\n");
        return false;	
	}
   
	// filtering
//	isoDiffusion(dst,data,width,height,lambda,sigma,iter2);
	anIsoDiffusion(dst,data,width,height,dt,threshold,iter,smoothings,a1,a2);


	// save , saveName
	FILE* pSave = fopen(saveName,"wb");
	if(pSave == NULL)
	{
		printf("Save file open error\n");
        return false;
	}

	fwrite(dst,sizeof(float),size,pSave);
	
	if(fclose(pSave)!=0)
	{
		printf("Save file close error\n");
        return false;	
	}
   
	delete [] data;
	delete [] dst;

	return 0;
}




void anIsoDiffusion(float* dst ,float* src, int w ,int h, float dt, float threshold, int nb_iter, int nb_smoothings, float a1, float a2)
{
    // variables & arrays
	float* grad  = new float[2*w*h];
	float* G     = new float[w*h*3];
	float* T     = new float[w*h*3];
    float* veloc = new float[w*h*3];
    float val1,val2;
	float vec1,vec2;
	double xdt;
	int iter;
	float fx, fy;
	double ipfnew;
	double average, stddev, drange, drange2, drange0;
   
	// consts
	const float c1 = (float)(0.25*(2-sqrt(2.0))), c2 = (float)(0.5f*(sqrt(2.0)-1));

	// get initial stats for later normalization
	float pix;
	float initial_max, initial_min;
	initial_max = initial_min = src[0];
	average = 0;
	for (int y=h; y-->0;)
		for(int x=w; x-->0;)
		{
			pix=src[y*w+x];
			if (pix>initial_max) initial_max=pix;
			if (pix<initial_min) initial_min=pix;
			average += pix;		
		}
	average /= (w*h);

	// standard deviation
    stddev = 0;
	for (int y=h; y-->0;)
		for(int x=w; x-->0;)
		{
			pix = src[y*w+x];
			stddev += (pix-average)*(pix-average);
        }
    stddev = sqrt(stddev/(w*h));

	// version 0.3 normalization
	drange= (threshold*stddev)/256.0 ;
	drange0= (6*stddev)/256.0;
	drange2= drange * drange;

    // iteration loop
    for(iter=0; iter < nb_iter ; iter++)
	{
		// compute gradients
        float Ipp,Icp,Inp, Ipc,Icc,Inc, Ipn,Icn,Inn;
		float IppInn,IpnInp;
		for(int y=h; y-->0;)
		{
			int py=y-1; if (py<0) py=0;
			int ny=y+1; if (ny==h) ny--;
			for(int x=w; x-->0;)
			{
				int px=x-1; if (px<0) px=0;
				int nx=x+1; if (nx==w) nx--;
				
				Ipp=src[py*w+ px];
				Ipc=src[y *w+ px];
				Ipn=src[ny*w+ px];
				Icp=src[py*w+  x];
				Icn=src[ny*w+  x];
				Inp=src[py*w+ nx];
				Inc=src[y *w+ nx];
				Inn=src[ny*w+ nx];
				IppInn = c1*(Inn-Ipp);
				IpnInp = c1*(Ipn-Inp);
				grad[y*2*w+ 2*x  ] = (float)(IppInn-IpnInp-c2*Ipc+c2*Inc);
				grad[y*2*w+ 2*x+1] = (float)(IppInn+IpnInp-c2*Icp+c2*Icn);               
			}
		} // Grad

		// compute structure tensor filed G
		for (int y=h; y-->0;)
			for (int x=w; x-->0;)
			{
				fx = grad[y*2*w+ 2*x  ];
				fy = grad[y*2*w+ 2*x+1];
				G[y*3*w+ 3*x  ] = fx*fx;
				G[y*3*w+ 3*x+1] = fx*fy;
				G[y*3*w+ 3*x+2] = fy*fy;
			}

		// compute the tensor field T
        for (int y=h; y-->0;)
			for(int x=w; x-->0;)
			{
				// eigenvalue
				double a= G[y*3*w+ 3*x  ];
				double b= G[y*3*w+ 3*x+1];
				double c= G[y*3*w+ 3*x+1];
				double d= G[y*3*w+ 3*x+2];
                double e= a+d;
                double f= sqrt(e*e-4*(a*d-b*c));
				double l1 = 0.5*(e-f), l2 = 0.5*(e+f);
		        if (e>0) { if (l1!=0) l2 = (a*d - b*c)/l1; }
				else     { if (l2!=0) l1 = (a*d - b*c)/l2; }
				
				val1=(float)(l2 / drange2);
				val2=(float)(l1 / drange2);
				
				// slight cheat speedup for default a1 value
				float f1 = (a1==.5) ? (float)(1/sqrt(1.0f+val1+val2)) : (float)(pow(1.0f+val1+val2,-a1));
				float f2 = (float)(pow(1.0f+val1+val2,-a2));

				double u, v, n;
				if (abs(b)>abs(a-l1)) { u = 1; v = (l1-a)/b; }
				else {
					if (a-l1!=0) { u = -b/(a-l1); v = 1; } 
					else         { u = 1; v = 0; } 
				}
				n = sqrt(u*u+v*v); u/=n; v/=n; 
				vec1 = (float)u; vec2 = (float)v;
				float vec11 = vec1*vec1, vec12 = vec1*vec2, vec22 = vec2*vec2;
				T[y*3*w+ 3*x  ] = f1*vec11 + f2*vec22;
				T[y*3*w+ 3*x+1] = (f1-f2)*vec12;
				T[y*3*w+ 3*x+2] = f1*vec22 + f2*vec11;
			} // tensor

		for(int sit=0; sit < nb_smoothings ; sit++)
		{
			// compute velocity
			for (int y=h; y-->0;) 
			{
				int py=y-1; if (py<0) py=0;
				int ny=y+1; if (ny==h) ny--;
				for (int x=w; x-->0;) 
				{
                    int px=x-1; if (px<0) px=0;
			     	int nx=x+1; if (nx==w) nx--;
					Ipp=src[py*w+ px];
					Ipc=src[ y*w+ px];
					Ipn=src[ny*w+ px];
					Icp=src[py*w+  x];
					Icc=src[ y*w+  x];
					Icn=src[ny*w+  x];
					Inp=src[py*w+ nx];
					Inc=src[ y*w+ nx];
					Inn=src[ny*w+ nx];
					float ixx = Inc+Ipc-2*Icc,
						iyy = Icn+Icp-2*Icc,
						ixy = 0.5f*(Ipp+Inn-Ipn-Inp);
					veloc[y*w +x] = T[y*3*w+ 3*x  ]*ixx + T[y*3*w+ 3*x+1]*ixy + T[y*3*w+ 3*x+2]*iyy; 
				
				} // velocity
			}
				// find xdt coefficient
			if (dt>0) 
			{
				float max=veloc[0], min=veloc[0];
				for (int y=h; y-->0;)
					for (int x=w; x-->0;)
					{
						if (veloc[y*w +x]>max) max=veloc[y*w +x];
						if (veloc[y*w +x]<min) min=veloc[y*w +x];
					}
				//version 0.2 normalization
				if(abs(max) >= abs(min)) xdt = dt/abs(max)*drange0;
				else  xdt = dt/abs(min)*drange0;
			}
			else xdt = -dt;
		
			// update image
			for (int y=h; y-->0;)
				for (int x=w; x-->0;)
				{
					ipfnew = src[y*w +x] + veloc[y*w +x]*xdt;
					src[y*w +x] = (float)ipfnew;
				}
        }//nb_smoothing
	} // iteration   
	memcpy(dst,src,w*h*sizeof(float));

    delete [] grad;
    delete [] G;
    delete [] T;
    delete [] veloc;

}



void isoDiffusion(float* dst ,float* src, int w ,int h, float lambda, float sigma, int iter)
{
	register int i, x, y;

	float k2 = sigma * sigma;

	float gradn, grads, grade, gradw;
	float gcn, gcs, gce, gcw;
	float tmp;

	// ????????? ??????????????? ?????? ????????? ????????? 2?????? ?????? ?????? ??????
	float** cpy = new float*[h];
	for( i = 0 ; i < h ; i++ )
	{
		cpy[i] = new float[w];
		memset(cpy[i], 0, sizeof(float)*w);
	}

	float** buf = new float*[h];
	for( i = 0 ; i < h ; i++ )
	{
		buf[i] = new float[w];
		memset(buf[i], 0, sizeof(float)*w);
	}

	// ?????? ????????? ????????? ??????
	for( y = 0 ; y < h ; y++ )
	for( x = 0 ; x < w ; x++ )
	{
		cpy[y][x] = buf[y][x] = src[y*w+x];
	}

	for( i = 0 ; i < iter ; i++ )
	{
		for( y = 1 ; y < h-1 ; y++ )
		for( x = 1 ; x < w-1 ; x++ )
		{
			tmp = cpy[y][x];

			gradn = cpy[y-1][x  ] - tmp;
			grads = cpy[y+1][x  ] - tmp;
			grade = cpy[y  ][x-1] - tmp;
			gradw = cpy[y  ][x+1] - tmp;

			gcn = gradn / (1.0f + gradn*gradn/k2);
			gcs = grads / (1.0f + grads*grads/k2);
			gce = grade / (1.0f + grade*grade/k2);
			gcw = gradw / (1.0f + gradw*gradw/k2);

			buf[y][x] = cpy[y][x] + lambda*(gcn + gcs + gce + gcw);
		}

		// ?????? ??????
		for( y = 0 ; y < h ; y++ )
			memcpy(cpy[y], buf[y], sizeof(float)*w);
	}

	// ?????? ????????? ??? ??????
	for( y = 0 ; y < h ; y++ )
	for( x = 0 ; x < w ; x++ )
	{
		dst[y*w+x] = ((buf[y][x]+ 0.5f < 0) ? 0 : ((buf[y][x]+ 0.5f > 65535) ? 65535 : buf[y][x]+ 0.5f));
	}

	// ?????? ????????? ????????? ??????
	for( i = 0 ; i < h ; i++ )
	{
		delete [] buf[i];
		delete [] cpy[i];
	}

	delete [] buf;
	delete [] cpy;
}
/*
void anIsoDiffusion2(float* dst ,float* src, int w ,int h, float dt, float threshold, int nb_iter)
{
    float* grad  = new float[2*w*h];   // macro function _grad(y,x,a,w) -> grad[a*w*y+a*x]
	float* G     = new float[3*w*h];
	float* T     = new float[3*w*h];
    float* veloc = new float[3*w*h];
    float val1,val2;x
	float vec1,vec2;
	double xdt;
	int iter;
	float fx, fy;
	double ipfnew;
	double average, stddev, drange, drange2, drange0;

	for(iter=0; iter < nb_iter ; iter++)
	{
		// compute gradients
        float Ipp,Icp,Inp, Ipc,Icc,Inc, Ipn,Icn,Inn;
		float IppInn,IpnInp;
		for(int y=h; y-->0;)
		{
			int py=y-1; if (py<0) py=0;
			int ny=y+1; if (ny==h) ny--;
			for(int x=w; x-->0;)
			{
				int px=x-1; if (px<0) px=0;
				int nx=x+1; if (nx==w) nx--;
				
				Ipp=_src(py,px,w);
				Ipc=_src( y,px,w);
				Ipn=_src(ny,px,w);
				Icp=_src(py, x,w);
				Icn=_src(ny, x,w);
				Inp=_src(py,nx,w);
				Inc=_src( y,nx,w);
				Inn=_src(ny,nx,w);
	

				IppInn = c1*(Inn-Ipp);
				IpnInp = c1*(Ipn-Inp);
				grad[y*2*w+ 2*x  ] = (float)(IppInn-IpnInp-c2*Ipc+c2*Inc);
				grad[y*2*w+ 2*x+1] = (float)(IppInn+IpnInp-c2*Icp+c2*Icn);               
			}
		} // Grad

	
	
	
	}

}*/

/*
// variables & arrays
  1.gredient 2*w*h
  2.tensor G 2*w*h
  3.tensor T 2*w*h
  4.velocity
  

// iteration loop
  1. // compute gradients
  2. // compute the tensor field T
       a. // eigen value
       b. // eigen vector
  3. // compute velocity
  4. // update image

// ?????????.
*/

