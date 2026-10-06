#include <algorithm>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gmsh.h>

#include "cmk.h"

namespace cmk {

namespace {

template <typename T>
void writeBinary(std::ofstream &out, const T *data, std::size_t count = 1)
{
    out.write(reinterpret_cast<const char *>(data), count * sizeof(T));
}

// Elements of one type in one entity, written under one physical group.
struct ElementBlock {
    int type;
    int numNodes;
    int physical;
    int entity;
    const std::vector<std::size_t> *connectivity;
    std::size_t count;
};

// Elements of one entity, as returned by gmsh.
struct EntityElements {
    std::vector<int> types;
    std::vector<std::vector<std::size_t>> elementTags, nodeTags;
};

void writePhysicalNames(std::ofstream &out, const std::vector<std::pair<int, int>> &physicals)
{
    if (physicals.empty())
        return;
    out << "$PhysicalNames\n" << physicals.size() << '\n';
    for (const auto &[dim, tag] : physicals) {
        std::string name;
        gmsh::model::getPhysicalName(dim, tag, name);
        out << dim << ' ' << tag << " \"" << name << "\"\n";
    }
    out << "$EndPhysicalNames\n";
}

// Nodes are written by increasing tag.
void writeNodes(std::ofstream &out, bool binary)
{
    std::vector<std::size_t> tags;
    std::vector<double> coord, param;
    gmsh::model::mesh::getNodes(tags, coord, param, -1, -1, false, false);

    std::vector<std::size_t> order(tags.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [&tags](std::size_t a, std::size_t b) { return tags[a] < tags[b]; });

    out << "$Nodes\n" << tags.size() << '\n';
    for (const auto i : order) {
        if (binary) {
            const int tag = static_cast<int>(tags[i]);
            writeBinary(out, &tag);
            writeBinary(out, &coord[3 * i], 3);
        }
        else {
            out << tags[i] << ' ' << coord[3 * i] << ' ' << coord[3 * i + 1] << ' '
                << coord[3 * i + 2] << '\n';
        }
    }
    if (binary)
        out << '\n';
    out << "$EndNodes\n";
}

// As in gmsh, an element belonging to several physical groups is written once
// per group; elements are numbered 1..N in the order they are written.
void writeElements(std::ofstream &out, bool binary, bool saveAll)
{
    std::vector<std::pair<int, int>> entities;
    gmsh::model::getEntities(entities);
    std::vector<EntityElements> elements(entities.size());
    std::vector<ElementBlock> blocks;
    std::size_t numElements = 0;

    for (std::size_t k = 0; k < entities.size(); ++k) {
        const auto [dim, tag] = entities[k];
        auto &e = elements[k];
        gmsh::model::mesh::getElements(e.types, e.elementTags, e.nodeTags, dim, tag);
        if (e.types.empty())
            continue;

        std::vector<int> physicals;
        gmsh::model::getPhysicalGroupsForEntity(dim, tag, physicals);
        if (physicals.empty()) {
            if (!saveAll)
                continue;
            physicals.push_back(0);
        }

        for (std::size_t t = 0; t < e.types.size(); ++t) {
            std::string name;
            int elementDim, order, numNodes, numPrimaryNodes;
            std::vector<double> localNodeCoord;
            gmsh::model::mesh::getElementProperties(e.types[t], name, elementDim, order,
                                                    numNodes, localNodeCoord, numPrimaryNodes);
            for (const int physical : physicals) {
                blocks.push_back({e.types[t], numNodes, physical, tag, &e.nodeTags[t],
                                  e.elementTags[t].size()});
                numElements += e.elementTags[t].size();
            }
        }
    }

    out << "$Elements\n" << numElements << '\n';
    int number = 0;
    for (const auto &b : blocks) {
        const auto &connectivity = *b.connectivity;
        if (binary) {
            const int header[3] = {b.type, static_cast<int>(b.count), 2};
            writeBinary(out, header, 3);
            std::vector<int> data(3 + b.numNodes);
            for (std::size_t e = 0; e < b.count; ++e) {
                data[0] = ++number;
                data[1] = b.physical;
                data[2] = b.entity;
                for (int n = 0; n < b.numNodes; ++n)
                    data[3 + n] = static_cast<int>(connectivity[e * b.numNodes + n]);
                writeBinary(out, data.data(), data.size());
            }
        }
        else {
            for (std::size_t e = 0; e < b.count; ++e) {
                out << ++number << ' ' << b.type << " 2 " << b.physical << ' ' << b.entity;
                for (int n = 0; n < b.numNodes; ++n)
                    out << ' ' << connectivity[e * b.numNodes + n];
                out << '\n';
            }
        }
    }
    if (binary)
        out << '\n';
    out << "$EndElements\n";
}

}  // namespace

void writeMsh22(const fs::path &file, bool binary)
{
    std::ofstream out(file, binary ? std::ios::out | std::ios::binary : std::ios::out);
    if (!out)
        throw std::runtime_error("cannot write file " + file.string());
    out.precision(16);

    double saveAllOption = 0;
    gmsh::option::getNumber("Mesh.SaveAll", saveAllOption);
    std::vector<std::pair<int, int>> physicals;
    gmsh::model::getPhysicalGroups(physicals);
    const bool saveAll = saveAllOption != 0 || physicals.empty();

    out << "$MeshFormat\n2.2 " << binary << ' ' << sizeof(double) << '\n';
    if (binary) {
        const int one = 1;
        writeBinary(out, &one);
        out << '\n';
    }
    out << "$EndMeshFormat\n";

    writePhysicalNames(out, physicals);
    writeNodes(out, binary);
    writeElements(out, binary, saveAll);

    if (!out)
        throw std::runtime_error("error while writing " + file.string());
}

}  // namespace cmk
