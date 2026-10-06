// Reverse Cuthill-McKee node renumbering of gmsh meshes.
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace cmk {

namespace fs = std::filesystem;

// Version and encoding read from the $MeshFormat header of an msh file.
struct MshFormat {
    double version = 4.1;
    bool binary = false;
};

// Node i is the node with the i-th smallest gmsh tag.
struct Renumbering {
    std::vector<std::size_t> nodeTags;  // gmsh tag of node i (sorted)
    std::vector<std::size_t> newIndex;  // position of node i in the RCM ordering
    std::vector<std::size_t> oldIndex;  // node placed at position j (inverse of newIndex)
    std::size_t bandwidthBefore = 0;
    std::size_t bandwidthAfter = 0;
};

// RAII wrapper around gmsh::initialize / gmsh::finalize.
class GmshSession {
public:
    GmshSession();
    ~GmshSession();
    GmshSession(const GmshSession &) = delete;
    GmshSession &operator=(const GmshSession &) = delete;
};

// Returns std::nullopt if the file is not in msh format.
std::optional<MshFormat> readMshFormat(const fs::path &file);

// Computes the RCM ordering of the mesh currently loaded in gmsh.
Renumbering reverseCuthillMcKee();

// Applies the renumbering to the loaded mesh: new tags are 1..N in RCM order.
void applyRenumbering(const Renumbering &r);

// Writes the loaded mesh; the format is driven by Mesh.MshFileVersion and
// Mesh.Binary for .msh files, by the extension otherwise.
void writeMesh(const fs::path &file);

// Writes the loaded mesh in msh 2.2 format, keeping node tags as they are.
void writeMsh22(const fs::path &file, bool binary);

}  // namespace cmk
