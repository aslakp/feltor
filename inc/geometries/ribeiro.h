#pragma once

#include "dg/backend/grid.h"
#include "dg/backend/functions.h"
#include "dg/backend/interpolation.cuh"
#include "dg/backend/operator.h"
#include "dg/backend/derivatives.h"
#include "dg/functors.h"
#include "dg/runge_kutta.h"
#include "dg/nullstelle.h"
#include "dg/geometry.h"
#include "fields.h"
#include "utilities.h"



namespace dg
{
namespace ribeiro
{

///@cond
namespace detail
{

//This leightweights struct and its methods finds the initial R and Z values and the coresponding f(\psi) as 
//good as it can, i.e. until machine precision is reached
template< class Psi, class PsiX, class PsiY>
struct Fpsi
{
    Fpsi( Psi psi, PsiX psiX, PsiY psiY, double x0, double y0): 
        psip_(psi), fieldRZYT_(psiX, psiY, x0, y0), fieldRZtau_(psiX, psiY)
    {
        R_init = x0; Z_init = y0;
        while( fabs( psiX(R_init, Z_init)) <= 1e-10 && fabs( psiY( R_init, Z_init)) <= 1e-10)
            R_init = x0 + 1.; Z_init = y0;
    }
    //finds the starting points for the integration in y direction
    void find_initial( double psi, double& R_0, double& Z_0) 
    {
        unsigned N = 50;
        thrust::host_vector<double> begin2d( 2, 0), end2d( begin2d), end2d_old(begin2d); 
        begin2d[0] = end2d[0] = end2d_old[0] = R_init;
        begin2d[1] = end2d[1] = end2d_old[1] = Z_init;
        //std::cout << "In init function\n";
        double eps = 1e10, eps_old = 2e10;
        while( (eps < eps_old || eps > 1e-7) && eps > 1e-14)
        {
            eps_old = eps; end2d_old = end2d;
            N*=2; dg::stepperRK17( fieldRZtau_, begin2d, end2d, psip_(R_init, Z_init), psi, N);
            eps = sqrt( (end2d[0]-end2d_old[0])*(end2d[0]-end2d_old[0]) + (end2d[1]-end2d_old[1])*(end2d[1]-end2d_old[1]));
        }
        R_init = R_0 = end2d_old[0], Z_init = Z_0 = end2d_old[1];
    }

    //compute f for a given psi between psi0 and psi1
    double construct_f( double psi, double& R_0, double& Z_0) 
    {
        find_initial( psi, R_0, Z_0);
        thrust::host_vector<double> begin( 3, 0), end(begin), end_old(begin);
        begin[0] = R_0, begin[1] = Z_0;
        //std::cout << begin[0]<<" "<<begin[1]<<" "<<begin[2]<<"\n";
        double eps = 1e10, eps_old = 2e10;
        unsigned N = 50;
        //double y_eps = 1;
        while( (eps < eps_old || eps > 1e-7)&& N < 1e6)
        {
            eps_old = eps, end_old = end;
            N*=2; dg::stepperRK17( fieldRZYT_, begin, end, 0., 2*M_PI, N);
            eps = sqrt( (end[0]-begin[0])*(end[0]-begin[0]) + (end[1]-begin[1])*(end[1]-begin[1]));
        }
        //std::cout << "\t error "<<eps<<" with "<<N<<" steps\t";
        //std::cout <<end_old[2] << " "<<end[2] << "error in y is "<<y_eps<<"\n";
        double f_psi = 2.*M_PI/end_old[2];
        return f_psi;
    }
    double operator()( double psi)
    {
        double R_0, Z_0; 
        return construct_f( psi, R_0, Z_0);
    }

    /**
     * @brief This function computes the integral x_1 = -\int_{\psi_0}^{\psi_1} f(\psi) d\psi to machine precision
     *
     * @param psi_0 lower boundary 
     * @param psi_1 upper boundary 
     *
     * @return x1
     */
    double find_x1( double psi_0, double psi_1 ) 
    {
        unsigned P=8;
        double x1 = 0, x1_old = 0;
        double eps=1e10, eps_old=2e10;
        std::cout << "In x1 function\n";
        while(eps < eps_old && P < 20 && eps > 1e-15)
        {
            eps_old = eps; 
            x1_old = x1;

            P+=1;
            dg::Grid1d<double> grid( psi_0, psi_1, P, 1);
            dg::Grid1d<double> grid_inv( psi_1, psi_0, P, 1);
            thrust::host_vector<double> psi_vec = psi_1>psi_0 ? dg::evaluate( dg::cooX1d, grid): dg::evaluate(dg::cooX1d, grid_inv);
            thrust::host_vector<double> f_vec(grid.size(), 0);
            thrust::host_vector<double> w1d = dg::create::weights(grid);
            for( unsigned i=0; i<psi_vec.size(); i++)
            {
                f_vec[i] = this->operator()( psi_vec[i]);
            }
            x1 = dg::blas1::dot( f_vec, w1d);

            eps = fabs((x1 - x1_old)/x1);
            std::cout << "X1 = "<<-x1<<" rel. error "<<eps<<" with "<<P<<" polynomials\n";
        }
        return -x1_old;
    }

    double f_prime( double psi) 
    {
        //compute fprime
        double deltaPsi = fabs(psi)/100.;
        double fofpsi[4];
        fofpsi[1] = operator()(psi-deltaPsi);
        fofpsi[2] = operator()(psi+deltaPsi);
        double fprime = (-0.5*fofpsi[1]+0.5*fofpsi[2])/deltaPsi, fprime_old;
        double eps = 1e10, eps_old=2e10;
        while( eps < eps_old)
        {
            deltaPsi /=2.;
            fprime_old = fprime;
            eps_old = eps;
            fofpsi[0] = fofpsi[1], fofpsi[3] = fofpsi[2];
            fofpsi[1] = operator()(psi-deltaPsi);
            fofpsi[2] = operator()(psi+deltaPsi);
            //reuse previously computed fpsi for current fprime
            fprime  = (+ 1./12.*fofpsi[0] 
                       - 2./3. *fofpsi[1]
                       + 2./3. *fofpsi[2]
                       - 1./12.*fofpsi[3]
                     )/deltaPsi;
            eps = fabs((fprime - fprime_old)/fprime);
            //std::cout << "fprime "<<fprime<<" rel error fprime is "<<eps<<" delta psi "<<deltaPsi<<"\n";
        }
        return fprime_old;
    }

    private:
    double R_init, Z_init;
    Psi psip_;
    solovev::ribeiro::FieldRZYT<PsiX, PsiY> fieldRZYT_;
    solovev::FieldRZtau<PsiX, PsiY> fieldRZtau_;
};

//This struct computes -2pi/f with a fixed number of steps for all psi
template<class Psi, class PsiR, class PsiZ>
struct FieldFinv
{
    FieldFinv( Psi psi, PsiR psiR, PsiZ psiZ, double x0, double y0, unsigned N_steps = 500):
        fpsi_(psi, psiR, psiZ, x0, y0), fieldRZYT_(psiR, psiZ, x0, y0), N_steps(N_steps) { }
    void operator()(const thrust::host_vector<double>& psi, thrust::host_vector<double>& fpsiM) 
    { 
        thrust::host_vector<double> begin( 3, 0), end(begin), end_old(begin);
        fpsi_.find_initial( psi[0], begin[0], begin[1]);
        //std::cout << begin[0]<<" "<<begin[1]<<" "<<begin[2]<<"\n";
        dg::stepperRK17( fieldRZYT_, begin, end, 0., 2*M_PI, N_steps);
        //eps = sqrt( (end[0]-begin[0])*(end[0]-begin[0]) + (end[1]-begin[1])*(end[1]-begin[1]));
        fpsiM[0] = end[2]/2./M_PI;
        //std::cout <<"fpsiMinverse is "<<fpsiM[0]<<" "<<-1./fpsi_(psi[0])<<" "<<eps<<"\n";
    }
    private:
    Fpsi<Psi, PsiR, PsiZ> fpsi_;
    solovev::ribeiro::FieldRZYT<PsiR, PsiZ> fieldRZYT_;
    unsigned N_steps;
};
} //namespace detail
}//namespace ribeiro

/**
 * @brief A two-dimensional grid based on "almost-ribeiro" coordinates by Ribeiro and Scott 2010
 */
template< class Psi, class PsiX, class PsiY, class PsiXX, class PsiXY, class PsiYY>
struct Ribeiro
{
    Ribeiro( Psi psi, PsiX psiX, PsiY psiY, PsiXX psiXX, PsiXY psiXY, PsiYY psiYY, double psi_0, double psi_1, double x0, double y0):
        psi_(psi), psiX_(psiX), psiY_(psiY), psiXX_(psiXX), psiXY_(psiXY), psiYY_(psiYY)
    {
        assert( psi_1 != psi_0);
        ribeiro::detail::Fpsi<Psi, PsiX, PsiY> fpsi(psi, psiX, psiY, x0, y0);
        lx_ = fabs(fpsi.find_x1( psi_0, psi_1));
        x0_=x0, y0_=y0, psi0_=psi_0, psi1_=psi_1;
        std::cout << "lx_ = "<<lx_<<"\n";
    }
    double width() const{return lx_;}
    double height() const{return 2.*M_PI;}
    thrust::host_vector<double> fx() const{ return fx_;}
    void operator()( 
         const thrust::host_vector<double>& zeta1d, 
         const thrust::host_vector<double>& eta1d, 
         thrust::host_vector<double>& x, 
         thrust::host_vector<double>& y, 
         thrust::host_vector<double>& zetaX, 
         thrust::host_vector<double>& zetaY, 
         thrust::host_vector<double>& etaX, 
         thrust::host_vector<double>& etaY) 
    {
        //compute psi(x) for a grid on x and call construct_rzy for all psi
        ribeiro::detail::FieldFinv<Psi, PsiX, PsiY> fpsiMinv_(psi_, psiX_, psiY_, x0_,y0_, 500);
        thrust::host_vector<double> psi_x;
        dg::detail::construct_psi_values( fpsiMinv_, psi0_, psi1_, 0., zeta1d, lx_, psi_x, fx_);

        std::cout << "In grid function:\n";
        ribeiro::detail::Fpsi<Psi, PsiX, PsiY> fpsi(psi_, psiX_, psiY_, x0_, y0_);
        solovev::ribeiro::FieldRZYRYZY<PsiX, PsiY, PsiXX, PsiXY, PsiYY> fieldRZYRYZY(psiX_, psiY_, psiXX_, psiXY_, psiYY_);
        unsigned size = zeta1d.size()*eta1d.size();
        x.resize(size), y.resize(size);
        zetaX = zetaY = etaX = etaY =x ;
        thrust::host_vector<double> f_p(fx_);
        unsigned Nx = zeta1d.size(), Ny = eta1d.size();
        for( unsigned i=0; i<zeta1d.size(); i++)
        {
            thrust::host_vector<double> ry, zy;
            thrust::host_vector<double> yr, yz, xr, xz;
            double R0, Z0;
            dg::detail::compute_rzy( fpsi, fieldRZYRYZY, psi_x[i], eta1d, ry, zy, yr, yz, xr, xz, R0, Z0, fx_[i], f_p[i]);
            for( unsigned j=0; j<Ny; j++)
            {
                x[j*Nx+i]  = ry[j], y[j*Nx+i]  = zy[j];
                etaX[j*Nx+i] = yr[j], etaY[j*Nx+i] = yz[j];
                zetaX[j*Nx+i] = xr[j], zetaY[j*Nx+i] = xz[j];
            }
        }
    }
    private:
    Psi psi_;
    PsiX psiX_;
    PsiY psiY_;
    PsiXX psiXX_;
    PsiXY psiXY_;
    PsiYY psiYY_;
    thrust::host_vector<double> fx_;
    double lx_, x0_, y0_, psi0_, psi1_;
};

///**
// * @brief Integrates the equations for a field line and 1/B
// */ 
//struct Field
//{
//    Field( solovev::GeomParameters gp,const thrust::host_vector<double>& x, const thrust::host_vector<double>& f_x):
//        gp_(gp),
//        psipR_(gp), psipZ_(gp),
//        ipol_(gp), invB_(gp), last_idx(0), x_(x), fx_(f_x)
//    { }
//
//    /**
//     * @brief \f[ \frac{d \hat{R} }{ d \varphi}  = \frac{\hat{R}}{\hat{I}} \frac{\partial\hat{\psi}_p}{\partial \hat{Z}}, \hspace {3 mm}
//     \frac{d \hat{Z} }{ d \varphi}  =- \frac{\hat{R}}{\hat{I}} \frac{\partial \hat{\psi}_p}{\partial \hat{R}} , \hspace {3 mm}
//     \frac{d \hat{l} }{ d \varphi}  =\frac{\hat{R}^2 \hat{B}}{\hat{I}  \hat{R}_0}  \f]
//     */ 
//    void operator()( const dg::HVec& y, dg::HVec& yp)
//    {
//        //x,y,s,R,Z
//        double psipR = psipR_(y[3],y[4]), psipZ = psipZ_(y[3],y[4]), ipol = ipol_( y[3],y[4]);
//        double fx = find_fx( y[0]);
//        yp[0] = 0;
//        yp[1] = fx*y[3]*(0.0+1.00*(psipR*psipR+psipZ*psipZ))/ipol;
//        yp[2] =  y[3]*y[3]/invB_(y[3],y[4])/ipol/gp_.R_0; //ds/dphi =  R^2 B/I/R_0_hat
//        yp[3] =  y[3]*psipZ/ipol;              //dR/dphi =  R/I Psip_Z
//        yp[4] = -y[3]*psipR/ipol;             //dZ/dphi = -R/I Psip_R
//
//    }
//    /**
//     * @brief \f[   \frac{1}{\hat{B}} = 
//      \frac{\hat{R}}{\hat{R}_0}\frac{1}{ \sqrt{ \hat{I}^2  + \left(\frac{\partial \hat{\psi}_p }{ \partial \hat{R}}\right)^2
//      + \left(\frac{\partial \hat{\psi}_p }{ \partial \hat{Z}}\right)^2}}  \f]
//     */ 
//    double operator()( double R, double Z) const { return invB_(R,Z); }
//    /**
//     * @brief == operator()(R,Z)
//     */ 
//    double operator()( double R, double Z, double phi) const { return invB_(R,Z,phi); }
//    double error( const dg::HVec& x0, const dg::HVec& x1)
//    {
//        //compute error in x,y,s
//        return sqrt( (x0[0]-x1[0])*(x0[0]-x1[0]) +(x0[1]-x1[1])*(x0[1]-x1[1])+(x0[2]-x1[2])*(x0[2]-x1[2]));
//    }
//    bool monitor( const dg::HVec& end){ 
//        if ( isnan(end[1]) || isnan(end[2]) || isnan(end[3])||isnan( end[4]) ) 
//        {
//            return false;
//        }
//        if( (end[3] < 1e-5) || end[3]*end[3] > 1e10 ||end[1]*end[1] > 1e10 ||end[2]*end[2] > 1e10 ||(end[4]*end[4] > 1e10) )
//        {
//            return false;
//        }
//        return true;
//    }
//    
//    private:
//    double find_fx(double x) 
//    {
//        if( fabs(x-x_[last_idx]) < 1e-12)
//            return fx_[last_idx];
//        for( unsigned i=0; i<x_.size(); i++)
//            if( fabs(x-x_[i]) < 1e-12)
//            {
//                last_idx = (int)i;
//                return fx_[i];
//            }
//        std::cerr << "x not found!!\n";
//        return 0;
//    }
//    
//    solovev::GeomParameters gp_;
//    solovev::PsipR  psipR_;
//    solovev::PsipZ  psipZ_;
//    solovev::Ipol   ipol_;
//    solovev::InvB   invB_;
//    int last_idx;
//    thrust::host_vector<double> x_;
//    thrust::host_vector<double> fx_;
//   
//};
//
} //namespace dg
