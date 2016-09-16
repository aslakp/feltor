#include <iostream>

#include "file/read_input.h"
#include "dg/backend/timer.cuh"
#include "dg/backend/grid.h"
#include "dg/elliptic.h"
#include "dg/cg.h"

#include "solovev.h"
//#include "guenther.h"
#include "conformal.h"



int main(int argc, char**argv)
{
    std::cout << "Type n, Nx, Ny, Nz\n";
    unsigned n, Nx, Ny, Nz;
    std::cin >> n>> Nx>>Ny>>Nz;   
    std::cout << "Type psi_0 and psi_1\n";
    double psi_0, psi_1;
    std::cin >> psi_0>> psi_1;
    std::vector<double> v, v2;
    try{ 
        if( argc==1)
        {
            v = file::read_input( "geometry_params_Xpoint.txt"); 
        }
        else
        {
            v = file::read_input( argv[1]); 
        }
    }
    catch (toefl::Message& m) {  
        m.display(); 
        for( unsigned i = 0; i<v.size(); i++)
            std::cout << v[i] << " ";
            std::cout << std::endl;
        return -1;}
    //write parameters from file into variables
    solovev::GeomParameters gp(v);
    gp.display( std::cout);
    dg::Timer t;
    std::cout << "Constructing grid ... \n";
    t.tic();
    solovev::Psip psip( gp); 
    solovev::PsipR psipR( gp); 
    solovev::PsipZ psipZ( gp); 
    solovev::LaplacePsip lap( gp); 
    hector::Hector<dg::IDMatrix, dg::DMatrix, dg::DVec> hector( psip, psipR, psipZ, lap, psi_0, psi_1, gp.R_0, 0.);
    for( unsigned i=0; i<5; i++)
    {
        conformal::RingGrid2d<dg::DVec> g2d(hector, n, Nx, Ny, dg::DIR);
        dg::Elliptic<conformal::RingGrid2d<dg::DVec>, dg::DMatrix, dg::DVec> pol( g2d, dg::not_normed, dg::centered);
        t.toc();
        std::cout << "Construction took "<<t.diff()<<"s\n";
        ///////////////////////////////////////////////////////////////////////////
        dg::DVec x =    dg::pullback( dg::zero, g2d);
        const dg::DVec b =    dg::pullback( solovev::EllipticDirPerM(gp, psi_0, psi_1), g2d);
        const dg::DVec chi =  dg::pullback( solovev::Bmodule(gp), g2d);
        const dg::DVec solution = dg::pullback( solovev::FuncDirPer(gp, psi_0, psi_1 ), g2d);
        const dg::DVec vol3d = dg::create::volume( g2d);
        pol.set_chi( chi);
        //compute error
        dg::DVec error( solution);
        const double eps = 1e-10;
        std::cout << "eps \t # iterations \t error \t hx_max\t hy_max \t time/iteration \n";
        std::cout << eps<<"\t";
        t.tic();
        dg::Invert<dg::DVec > invert( x, n*n*Nx*Ny*Nz, eps);
        unsigned number = invert(pol, x,b);
        std::cout <<number<<"\t";
        t.toc();
        dg::blas1::axpby( 1.,x,-1., solution, error);
        double err = dg::blas2::dot( vol3d, error);
        const double norm = dg::blas2::dot( vol3d, solution);
        std::cout << sqrt( err/norm) << "\t";
        dg::DVec gyy = g2d.g_xx(), gxx=g2d.g_yy(), vol = g2d.vol();
        dg::blas1::transform( gxx, gxx, dg::SQRT<double>());
        dg::blas1::transform( gyy, gyy, dg::SQRT<double>());
        dg::blas1::pointwiseDot( gxx, vol, gxx);
        dg::blas1::pointwiseDot( gyy, vol, gyy);
        dg::blas1::scal( gxx, g2d.hx());
        dg::blas1::scal( gyy, g2d.hy());
        std::cout << *thrust::max_element( gxx.begin(), gxx.end()) << "\t";
        std::cout << *thrust::max_element( gyy.begin(), gyy.end()) << "\t";
        std::cout<<t.diff()/(double)number<<"s"<<std::endl;
        Nx*=2; Ny*=2;
    }

    return 0;
}
