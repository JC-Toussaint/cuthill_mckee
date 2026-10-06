# cmk — Reverse Cuthill-McKee renumbering of gmsh meshes

`cmk` renumbers the nodes of a gmsh mesh with the Reverse Cuthill-McKee
algorithm (Boost.Graph) to reduce the bandwidth of the associated matrices.

The mesh is read and written through the gmsh C++ API, so every format gmsh
can read is supported (msh 2.2, 4.x, ascii or binary, ...). A `.msh` file is
written back in the same version and encoding as the input.

## Build

Requirements: a C++17 compiler, Boost (Graph), gmsh SDK (`gmsh.h`, `libgmsh`).

```sh
make                      # gmsh installed under /usr
make GMSH_DIR=/opt/gmsh   # gmsh SDK installed elsewhere
```

## Usage

```sh
./cmk mesh.msh
```

`mesh.msh` is replaced by the renumbered mesh; the original file is kept as
`mesh.msh.orig`. The bandwidth before and after renumbering is printed:

```
Format msh 4.1 ascii
Nodes : 662
  type 2 (Triangle 3) : 1316 elements
  type 4 (Tetrahedron 4) : 1855 elements
Elements : 3171

Original Bandwidth: 658
Final Bandwidth: 14
Wrote mesh.msh (original: mesh.msh.orig)
```

## Notes

- Only the mesh and its physical groups are written: post-processing sections
  (`$NodeData`, `$ElementData`, ...) and `$Periodic` are not kept.
- msh 2.2 files are written by `cmk` itself, because the gmsh msh 2.2 writer
  renumbers the nodes entity by entity, which would undo the renumbering.
  As in gmsh, elements are numbered 1..N and an element belonging to several
  physical groups is written once per group.
