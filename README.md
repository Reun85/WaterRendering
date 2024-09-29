# Water Simulation using FFT

This is a simple water simulation using Fast Fourier Transform (FFT) in C++ using DirectX as a rendering backend.
The simulation is heavily based on the paper "Simulating Ocean Water" by Jerry Tessendorf. The paper can be found [here](https://www.researchgate.net/publication/264839743_Simulating_Ocean_Water)

## Building

The project uses Visual Studio 2022 as a build manager. The required Visual studio features are:
- Desktop development with C++ (c++20 is used)
- Game development with C++ (DirectX sdk)
- Windows app development with C++ (Windows SDK)

The project only supports Windows 11.

The startup project should be Axodox.Graphics.Test

# Implementation details

The project implements the Fast Fourier Transform on the GPU to handle millions of waves affecting the ocean surface.
The waves are generated using the Phillips spectrum and the Gerstner waves on the CPU.
For spatial fractioning I use a non uniform tesselation. The Quadtree building with frustum culling is done on the CPU. The implementation is based on the paper "Tessellated Terrain Rendering with Dynamic LOD" by Victor Bush. The paper can be found [here](https://victorbush.com/2015/01/tessellated-terrain/)

For rendering I use Deferred Shading with a PBR pipeline for the best possible looking ocean surface.


## Future works

Finish the shadow volume implementation.
Move spektrum generation to the GPU.
Simulate (i.e., approximate) buoyancy by fitting a plane to the surface.
