#include "zigzag_2nd.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "../../tools/iter/iter.h"

#ifdef GPU
#include <nvtx3/nvToolsExt.h> 
#endif

using std::min;
using std::max;


// Triangular 2nd order charge fluxes
//inline void W2nd(double x, double xr, int i, double* out)
//{
//    // W_{i+1)^(1) 
//    //double xm = 0.5*(x + xr) - i; 
//    double xm = 0.5*(x + xr) - i; 
//
//    out[0] = xm;
//      
//    // charge fluxes for triangular shapes
//    out[1] = 0.50*(0.5 - xm)*(0.5 - xm); //W2_im1 
//    out[2] = 0.75 - xm*xm;                 //W2_i   
//    out[3] = 0.50*(0.5 + xm)*(0.5 + xm); //W2_ip1 
//}

inline void W2nd(double d, double* coeff){

  //NOTE: d at wings includes +-1 so W2_mp1 -> 0.5*(3/2 +- d)^2
  coeff[0] = 0.50*(0.5 - d)*(0.5 - d); //W2_im1 
  coeff[1] = 0.75 - d*d;                 //W2_i   
  coeff[2] = 0.50*(0.5 + d)*(0.5 + d); //W2_ip1 
}




template<size_t D, size_t V>
void pic::ZigZag_2nd<D,V>::solve( pic::Tile<D>& tile )
{

#ifdef GPU
  nvtxRangePush(__PRETTY_FUNCTION__);
#endif

  auto& yee = tile.get_yee();
  const auto mins = tile.mins;

  //clear arrays before new update
  yee.jx.clear();
  yee.jy.clear();
  yee.jz.clear();
  yee.rho.clear();


  for(auto&& con : tile.containers) {

    const double c = tile.cfl;    // speed of light
    const double q = con.q; // charge

    //UniIter::iterate([=] DEVCALLABLE (
    //            size_t n, 
    //            fields::YeeLattice &yee,
    //            pic::ParticleContainer<D>& con
    //            ){
    for(size_t n=0; n<con.size(); n++) {

      //--------------------------------------------------
      double u = con.vel(0,n);
      double v = con.vel(1,n);
      double w = con.vel(2,n);
      double invgam = 1.0/sqrt(1.0 + u*u + v*v + w*w);


      double gamx = sqrt(1.0 + u*u);
      double gamy = sqrt(1.0 + v*v);
      double gamz = sqrt(1.0 + w*w);

      //--------------------------------------------------
      // new (normalized) location, x_{n+1}
        
      //double x1, y1, z1;
      double x2 = D >= 1 ? con.loc(0,n) - mins[0] : con.loc(0,n);
      double y2 = D >= 2 ? con.loc(1,n) - mins[1] : con.loc(1,n);
      double z2 = D >= 3 ? con.loc(2,n) - mins[2] : con.loc(2,n);

      // previos location, x_n
      double x1 = x2 - u*invgam*c;
      double y1 = y2 - v*invgam*c;
      double z1 = z2 - w*invgam*c; 

      //--------------------------------------------------
      int i1  = D >= 1 ? floor(x1) : 0;
      int i2  = D >= 1 ? floor(x2) : 0;
      int j1  = D >= 2 ? floor(y1) : 0;
      int j2  = D >= 2 ? floor(y2) : 0;
      int k1  = D >= 3 ? floor(z1) : 0;
      int k2  = D >= 3 ? floor(z2) : 0;

      // 1st order relay point; +1 is equal to +\Delta x
      //double xr = min( double(min(i1,i2)+1), max( double(max(i1,i2)), double(0.5*(x1+x2)) ) );
      //double yr = min( double(min(j1,j2)+1), max( double(max(j1,j2)), double(0.5*(y1+y2)) ) );
      //double zr = min( double(min(k1,k2)+1), max( double(max(k1,k2)), double(0.5*(z1+z2)) ) );
        
      // 2nd order relay point; +1 is equal to +\Delta x
      double xr = min( double(min(i1,i2)+1), max( double(i1+i2)*0.5, double(0.5*(x1+x2)) ) );
      double yr = min( double(min(j1,j2)+1), max( double(j1+j2)*0.5, double(0.5*(y1+y2)) ) );
      double zr = min( double(min(k1,k2)+1), max( double(k1+k2)*0.5, double(0.5*(z1+z2)) ) );

      //--------------------------------------------------

      // \Delta x from grid points
      double dx1 = D >= 1 ? 0.5*(x1 + xr) - i1 : 0.5; 
      double dx2 = D >= 1 ? 0.5*(x2 + xr) - i2 : 0.5; 
                                                    
      double dy1 = D >= 2 ? 0.5*(y1 + yr) - j1 : 0.5; 
      double dy2 = D >= 2 ? 0.5*(y2 + yr) - j2 : 0.5; 
                                                    
      double dz1 = D >= 3 ? 0.5*(z1 + zr) - k1 : 0.5; 
      double dz2 = D >= 3 ? 0.5*(z2 + zr) - k2 : 0.5; 

      //--------------------------------------------------
      // Lorentz contract lenghts
      double gam = sqrt(1.0 + u*u + v*v + w*w); // \gamma
      double betax2 = pow(u/gam, 2);            // v_x^2
      double betay2 = pow(v/gam, 2);            // v_y^2
      double betaz2 = pow(w/gam, 2);            // v_z^2
      double beta2  = betax2 + betay2 + betaz2; // |v|^2

      dx1 = dx1/(1.0 + (gam-1.0)*betax2/beta2);
      dx2 = dx2/(1.0 + (gam-1.0)*betax2/beta2);
                
      dy1 = dy1/(1.0 + (gam-1.0)*betay2/beta2);
      dy2 = dy2/(1.0 + (gam-1.0)*betay2/beta2);
                
      dz1 = dz1/(1.0 + (gam-1.0)*betaz2/beta2);
      dz2 = dz2/(1.0 + (gam-1.0)*betaz2/beta2);

      //--------------------------------------------------
      // particle weights 
      double Wxx1[3] = {1.0}, 
             Wxx2[3] = {1.0}, 
             Wyy1[3] = {1.0}, 
             Wyy2[3] = {1.0}, 
             Wzz1[3] = {1.0}, 
             Wzz2[3] = {1.0};

      if(D >= 1) W2nd(dx1, Wxx1);
      if(D >= 1) W2nd(dx2, Wxx2);

      if(D >= 2) W2nd(dy1, Wyy1);
      if(D >= 2) W2nd(dy2, Wyy2);

      if(D >= 3) W2nd(dz1, Wzz1);
      if(D >= 3) W2nd(dz2, Wzz2);

      //reduce dimension if needed
      //if(D <= 1) {
      //  Wy1a, Wy2a, Wy3a = 1.0;
      //  Wy1b, Wy2b, Wy3b = 1.0;
      //}
      //if(D <= 2) {
      //  Wz1a, Wz2a, Wz3a = 1.0;
      //  Wz1b, Wz2b, Wz3b = 1.0;
      //}


      //Wx1_ip1 = 0.5f*(x + xr) - i; 
      //jx(i-1, j-1, k) = qvx * (0.5 - Wx1_ip1)*W2_jm1
      //jx(i  , j-1, k) = qvx * (0.5 + Wx1_ip1)*W2_jm1
      //jx(i-1, j  , k) = qvx * (0.5 - Wx1_ip1)*W2_j  
      //jx(i  , j  , k) = qvx * (0.5 + Wx1_ip1)*W2_j  
      //jx(i-1, j+1, k) = qvx * (0.5 - Wx1_ip1)*W2_jp1
      //jx(i  , j+1, k) = qvx * (0.5 + Wx1_ip1)*W2_jp1

      //--------------------------------------------------
      // q v = q (x_{i+1} - x_i)/dt
      //
      // NOTE: +q since - sign is already included in the Ampere's equation
      // NOTE: extra c to introduce time step immediately; therefore we store on grid J -> J\Delta t
      // NOTE: More generally we should have: q = weight*qe;
      double qvx1 = D >= 1 ? +q*(xr - x1) : +q*c;
      double qvy1 = D >= 2 ? +q*(yr - y1) : +q*c;
      double qvz1 = D >= 3 ? +q*(zr - z1) : +q*c;

      double qvx2 = D >= 1 ? +q*(x2 - xr) : +q*c;
      double qvy2 = D >= 2 ? +q*(y2 - yr) : +q*c;
      double qvz2 = D >= 3 ? +q*(z2 - zr) : +q*c;


      //--------------------------------------------------
      // dimension dependent loop boundaries
      const int xlim = D >= 1 ? 1 : 0;
      const int ylim = D >= 2 ? 1 : 0;
      const int zlim = D >= 3 ? 1 : 0;

      // NOTE: incrementing loop counter with ++i to enforce at least 1 iteration always

      //jx
      for(int zi=-zlim; zi <=zlim; ++zi)
      for(int yi=-ylim; yi <=ylim; ++yi){
        //std::cout << "jx: injecting into" <<
        //"(" << i1-1 <<","<< j1+yi <<","<< k1+zi <<") " <<
        //"(" << i1   <<","<< j1+yi <<","<< k1+zi <<") " <<
        //"(" << i2-1 <<","<< j2+yi <<","<< k2+zi <<") " <<
        //"(" << i2   <<","<< j2+yi <<","<< k2+zi <<") " << "\n";

        //first part of trajectory
        if(D >= 1) atomic_add( yee.jx(i1-1, j1+yi, k1+zi), qvx1*(0.5 - dx1)*Wyy1[yi+1]*Wzz1[zi+1] );
        if(D >= 1) atomic_add( yee.jx(i1  , j1+yi, k1+zi), qvx1*(0.5 + dx1)*Wyy1[yi+1]*Wzz1[zi+1] );

        //second part of trajectory
        if(D >= 1) atomic_add( yee.jx(i2-1, j2+yi, k2+zi), qvx2*(0.5 - dx2)*Wyy2[yi+1]*Wzz2[zi+1] );
        if(D >= 1) atomic_add( yee.jx(i2,   j2+yi, k2+zi), qvx2*(0.5 + dx2)*Wyy2[yi+1]*Wzz2[zi+1] );
      }

      //jy
      for(int zi=-zlim; zi <=zlim; ++zi)
      for(int xi=-xlim; xi <=xlim; ++xi){
        //std::cout << "jy: injecting into" <<
        //"(" << i1+xi <<","<< j1-1 <<","<< k1+zi <<") " <<
        //"(" << i1+xi <<","<< j1   <<","<< k1+zi <<") " <<
        //"(" << i2+xi <<","<< j2-1 <<","<< k2+zi <<") " <<
        //"(" << i2+xi <<","<< j2   <<","<< k2+zi <<") " << "\n";

        if(D >= 2) atomic_add( yee.jy(i1+xi, j1-1, k1+zi), qvy1*(0.5 - dy1)*Wxx1[xi+1]*Wzz1[zi+1] );
        if(D >= 1) atomic_add( yee.jy(i1+xi, j1,   k1+zi), qvy1*(0.5 + dy1)*Wxx1[xi+1]*Wzz1[zi+1] );

        if(D >= 2) atomic_add( yee.jy(i2+xi, j2-1, k2+zi), qvy2*(0.5 - dy2)*Wxx2[xi+1]*Wzz2[zi+1] );
        if(D >= 1) atomic_add( yee.jy(i2+xi, j2,   k2+zi), qvy2*(0.5 + dy2)*Wxx2[xi+1]*Wzz2[zi+1] );
      }                                                                                
                                                                                       
      //jz                                                                             
      for(int yi=-ylim; yi <=ylim; ++yi)                                                     
      for(int xi=-xlim; xi <=xlim; ++xi){                                                    
        //std::cout << "jz: injecting into" <<                                         
        //"(" << i1+xi <<","<< j1+yi <<","<< k1-1 <<") " <<                            
        //"(" << i1+xi <<","<< j1+yi <<","<< k1   <<") " <<                            
        //"(" << i2+xi <<","<< j2+yi <<","<< k2-1 <<") " <<                            
        //"(" << i2+xi <<","<< j2+yi <<","<< k2   <<") " << "\n";                      
                                                                                       
        if(D >= 3) atomic_add( yee.jz(i1+xi, j1+yi, k1-1), qvz1*(0.5 - dz1)*Wxx1[xi+1]*Wyy1[yi+1] );
        if(D >= 1) atomic_add( yee.jz(i1+xi, j1+yi, k1  ), qvz1*(0.5 + dz1)*Wxx1[xi+1]*Wyy1[yi+1] );

        if(D >= 3) atomic_add( yee.jz(i2+xi, j2+yi, k2-1), qvz2*(0.5 - dz2)*Wxx2[xi+1]*Wyy2[yi+1] );
        if(D >= 1) atomic_add( yee.jz(i2+xi, j2+yi, k2  ), qvz2*(0.5 + dz2)*Wxx2[xi+1]*Wyy2[yi+1] );
      }

    }
    //}, con.size(), yee, con);

  }//end of loop over species

}


//--------------------------------------------------
// explicit template instantiation
template class pic::ZigZag_2nd<1,3>; // 1D3V
template class pic::ZigZag_2nd<2,3>; // 2D3V
template class pic::ZigZag_2nd<3,3>; // 3D3V

