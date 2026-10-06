#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gmsh.h>

#include "cmk.h"

namespace cmk {

GmshSession::GmshSession() { gmsh::initialize(); }
GmshSession::~GmshSession() { gmsh::finalize(); }

// The $MeshFormat header is always ascii, even in a binary file.
std::optional<MshFormat> readMshFormat(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    std::string line;
    if (!std::getline(in, line) || line.rfind("$MeshFormat", 0) != 0)
        return std::nullopt;
    if (!std::getline(in, line))
        return std::nullopt;

    std::istringstream iss(line);
    MshFormat format;
    int binary = 0, dataSize = 0;
    if (!(iss >> format.version >> binary >> dataSize))
        return std::nullopt;
    format.binary = binary != 0;
    return format;
}

}  // namespace cmk

namespace {

// When physical groups exist, gmsh only writes the elements belonging to
// them by default: force Mesh.SaveAll if the input file also contained
// elements outside any physical group, so that nothing is lost.
void keepElementsOutsidePhysicalGroups()
{
    std::vector<std::pair<int, int>> physicals;
    gmsh::model::getPhysicalGroups(physicals);
    if (physicals.empty())
        return;

    std::vector<std::pair<int, int>> entities;
    gmsh::model::getEntities(entities);
    for (const auto &[dim, tag] : entities) {
        std::vector<int> types;
        std::vector<std::vector<std::size_t>> elementTags, nodeTags;
        gmsh::model::mesh::getElements(types, elementTags, nodeTags, dim, tag);
        if (types.empty())
            continue;

        std::vector<int> physicalTags;
        gmsh::model::getPhysicalGroupsForEntity(dim, tag, physicalTags);
        if (physicalTags.empty()) {
            gmsh::option::setNumber("Mesh.SaveAll", 1);
            return;
        }
    }
}

void run(const cmk::fs::path &file)
{
    if (!cmk::fs::is_regular_file(file))
        throw std::runtime_error("cannot read file " + file.string());

    // Never overwrite a backup: it may hold the only copy of the original mesh.
    auto backup = file;
    backup += ".orig";
    if (cmk::fs::exists(backup))
        throw std::runtime_error("backup " + backup.string() + " already exists, "
                                 "remove or rename it first");

    cmk::GmshSession session;
    gmsh::option::setNumber("General.Terminal", 1);

    const auto format = cmk::readMshFormat(file);
    gmsh::open(file.string());

    if (format) {
        std::cout << "Format msh " << format->version
                  << (format->binary ? " binary" : " ascii") << '\n';
        gmsh::option::setNumber("Mesh.MshFileVersion", format->version);
        gmsh::option::setNumber("Mesh.Binary", format->binary);
    }
    keepElementsOutsidePhysicalGroups();

    const auto renumbering = cmk::reverseCuthillMcKee();
    std::cout << "\nOriginal Bandwidth: " << renumbering.bandwidthBefore
              << "\nFinal Bandwidth: " << renumbering.bandwidthAfter << '\n';

    cmk::applyRenumbering(renumbering);

    // Write to a temporary file first so that the input is left untouched if
    // writing fails; it keeps the extension, from which gmsh picks the format.
    auto temporary = file;
    temporary.replace_extension(".tmp" + file.extension().string());
    try {
        cmk::writeMesh(temporary);
    }
    catch (...) {
        std::error_code ignored;
        cmk::fs::remove(temporary, ignored);
        throw;
    }

    cmk::fs::rename(file, backup);
    cmk::fs::rename(temporary, file);
    std::cout << "Wrote " << file.string() << " (original: " << backup.string() << ")\n";
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: cmk file\n";
        return EXIT_FAILURE;
    }

    try {
        run(argv[1]);
    }
    catch (const std::exception &e) {
        std::cerr << "cmk: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
