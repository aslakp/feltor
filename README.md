# Welcome to the FELTOR project!

![3dsimulation](3dpic.jpg)

FELTOR (Full-F ELectromagnetic code in TORoidal geometry) is both a numerical library and a scientific software package built on top of it.

Its main physical target are plasma edge and scrape-off layer (gyro-)fluid simulations.
The numerical methods centre around discontinuous Galerkin methods on structured grids. 
Our lowest core level functions are parallelized for a variety of hardware from multi-core cpu to hybrid MPI+GPU, which makes them incredibly fast. 



## Quick start guide
Go ahead and clone our library into any folder you like 
```sh
$ git clone https://www.github.com/feltor-dev/feltor
```
You also need to clone  [thrust]( https://github.com/thrust/thrust) and [cusp](https://github.com/cusplibrary/cusplibrary) distributed under the Apache-2.0 license. So again in a folder of your choice
```sh
$ git clone https://www.github.com/thrust/thrust
$ git clone https://www.github.com/cusplibrary/cusplibrary
```
> Our code only depends on external libraries that are themselves openly available. We note here that we do not distribute copies of these libraries.

Now you need to tell the feltor configuration where these external libraries are located on your computer. The default way to do this is to go in your `HOME` directory, make an include directory and link the paths in this directory:
 ```sh
 $ cd ~
 $ mkdir include
 $ cd include
 $ ln -s path/to/thrust/thrust thrust
 $ ln -s path/to/cusplibrary/cusp cusp
```
> If you do not like this, you can also create your own config file as discribed [here](https://github.com/feltor-dev/feltor/wiki/Configuration).

Now let us compile the first benchmark program. 
 

 ```sh
 $ cd path/to/feltor/inc/dg
 
 $ make blas_b device=omp #(for an OpenMP version)
 #or
 $ make blas_b device=gpu #(if you have a gpu and nvcc )
 ```
> The minimum requirement to compile and run an application is a working C++ compiler (g++ per default) and a CPU. 
> To simplify the compilation process we use the GNU Make utility, a standard build automation tool that automatically builds the executable program. 
> We don't use new C++-11 standard features to avoid complications since some clusters are a bit behind on up-to-date compilers.
> The OpenMP standard is natively supported by most recent C++ compilers.   
> Our GPU backend uses the [Nvidia-CUDA](https://developer.nvidia.com/cuda-zone) programming environment and in order to compile and run a program for a GPU a user needs the nvcc CUDA compiler (available free of charge) and a NVidia GPU. However, we explicitly note here that due to the modular design of our software a user does not have to possess a GPU nor the nvcc compiler. The CPU version of the backend is equally valid and provides the same functionality.


Run the code with 
```sh
$ ./blas_b 
```
and when prompted for input vector sizes type for example
`3 100 100 10`
which makes a grid with 3 polynomial coefficients, 100 cells in x, 100 cells in y and 10 in z. 
> Go ahead and play around and see what happens. You can compile and run any other program that ends in `_t.cu` or `_b.cu` in `feltor/inc/dg` in this way. 

Now, let us test the mpi setup 
> You can of course skip this if you don't have mpi installed on your computer.
> If you intend to use the MPI backend, an implementation library of the mpi standard is required.

```sh
 $ cd path/to/feltor/inc/dg
 
 $ make blas_mpib device=omp  # (for MPI+OpenMP)
 # or
 $ make blas_mpib device=gpu # (for MPI+GPU)
 ```
Run the code with
`$ mpirun -n '# of procs' ./blas_mpib `
then tell how many process you want to use in the x-, y- and z- direction, for example:
`2 2 1` (i.e. 2 procs in x, 2 procs in y and 1 in z; total number of procs is 4)
when prompted for input vector sizes type for example
`3 100 100 10` (number of cells divided by number of procs must be an integer number)

Now, we want to compile a simulation program. First, we have to download and install some libraries for I/O-operations.
> For data output we use the [NetCDF](http://www.unidata.ucar.edu/software/netcdf/) library under an MIT - like license. The underlying [HDF5](https://www.hdfgroup.org/HDF5/) library also uses a very permissive license. Note that for the mpi versions of applications you need to build hdf5 and netcdf with the --enable-parallel flag. Do NOT use the pnetcdf library, which uses the classic netcdf file format.  
> Our JSON input files are parsed by [JsonCpp](https://www.github.com/open-source-parsers/jsoncpp) distributed under the MIT license (the 0.y.x branch to avoid C++-11 support).     
> Some desktop applications in FELTOR use the [draw library]( https://github.com/mwiesenberger/draw) (developed by us also under MIT), which depends on OpenGL (s.a. [installation guide](http://en.wikibooks.org/wiki/OpenGL_Programming)) and [glfw](http://www.glfw.org), an OpenGL development library under a BSD-like license. You don't need these when you are on a cluster. 

 
As in Step 3 you need to create links to the draw and the jsoncpp library in your include folder or provide the paths in your config file. 

```sh
 $ cd path/to/feltor/src/toefl
 
 $ make toeflR device=gpu # (for a live simulation on gpu with glfw output)
 # or
 $ make toefl_hpc device=gpu # (for a simulation on gpu with output stored to disc)
 # or
 $ make toefl_mpi device=omp  # (for an mpi simulation with output stored to disc)
```
A default input file is located in `path/to/feltor/src/toefl/input`
> The mpi program will wait for you to type the number of processes in x and y direction before starting

## Further reading
Please check out our [Wiki pages](https://github.com/feltor-dev/feltor/wiki) for some general information and user oriented documentation. The [developer oriented documentation](http://feltor-dev.github.io/feltor/inc/dg/html/modules.html) is generated with [Doxygen](http://www.doxygen.org) from source code.

## Official releases 
Our latest code release has a shiny DOI badge from zenodo

[![DOI](https://zenodo.org/badge/14143578.svg)](https://zenodo.org/badge/latestdoi/14143578)

which makes us officially citable.

## Contributions and Acknowledgements
For instructions on how to contribute read the [wiki page](https://github.com/feltor-dev/feltor/wiki/Contributions).
We gratefully acknowledge contributions from 
- Ralph Kube
- Eduard Reiter
- Lukas Einkemmer
## License 
FELTOR is free software and licensed under the very permissive MIT license. It was originally developed by Matthias Wiesenberger and Markus Held.

