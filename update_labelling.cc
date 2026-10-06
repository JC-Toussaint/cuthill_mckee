#include <cstddef>
#include <vector>

#include <gmsh.h>

#include "cmk.h"

namespace cmk {

void applyRenumbering(const Renumbering &r)
{
    std::vector<std::size_t> newTags(r.newIndex.size());
    for (std::size_t i = 0; i < newTags.size(); ++i)
        newTags[i] = r.newIndex[i] + 1;
    gmsh::model::mesh::renumberNodes(r.nodeTags, newTags);
}

// The gmsh msh 2.2 writer always renumbers the nodes entity by entity
// (ignoring their tags), which would undo the renumbering: msh 2.2 files are
// therefore written by writeMsh22.
void writeMesh(const fs::path &file)
{
    double version = 0, binary = 0;
    gmsh::option::getNumber("Mesh.MshFileVersion", version);
    gmsh::option::getNumber("Mesh.Binary", binary);

    if (file.extension() == ".msh" && version < 3)
        writeMsh22(file, binary != 0);
    else
        gmsh::write(file.string());
}

}  // namespace cmk
