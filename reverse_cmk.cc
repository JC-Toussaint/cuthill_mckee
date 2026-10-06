#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/bandwidth.hpp>
#include <boost/graph/cuthill_mckee_ordering.hpp>
#include <boost/graph/properties.hpp>

#include <gmsh.h>

#include "cmk.h"

namespace cmk {

namespace {

using Graph = boost::adjacency_list<
    boost::vecS, boost::vecS, boost::undirectedS,
    boost::property<boost::vertex_color_t, boost::default_color_type,
                    boost::property<boost::vertex_degree_t, int>>>;
using Vertex = boost::graph_traits<Graph>::vertex_descriptor;

// getNodes returns the nodes entity by entity: sort them by increasing tag
// so that the graph vertex index follows the numbering of the file.
std::vector<std::size_t> sortedNodeTags()
{
    std::vector<std::size_t> tags;
    std::vector<double> coord, param;
    gmsh::model::mesh::getNodes(tags, coord, param, -1, -1, false, false);
    std::sort(tags.begin(), tags.end());
    return tags;
}

// Connects every pair of nodes sharing an element. Neighbouring elements share
// edges: duplicates are removed so that vertex degrees, which drive the
// Cuthill-McKee ordering, count distinct neighbours.
Graph buildNodeGraph(const std::vector<std::size_t> &nodeTags)
{
    // gmsh tags are not necessarily contiguous (msh 4.x)
    std::unordered_map<std::size_t, std::size_t> tagToIndex;
    tagToIndex.reserve(nodeTags.size());
    for (std::size_t i = 0; i < nodeTags.size(); ++i)
        tagToIndex[nodeTags[i]] = i;

    std::vector<std::pair<std::size_t, std::size_t>> edges;

    std::vector<int> elementTypes;
    std::vector<std::vector<std::size_t>> elementTags, elementNodeTags;
    gmsh::model::mesh::getElements(elementTypes, elementTags, elementNodeTags, -1, -1);

    std::size_t numElements = 0;
    for (std::size_t t = 0; t < elementTypes.size(); ++t) {
        std::string name;
        int dim, order, numNodes, numPrimaryNodes;
        std::vector<double> localNodeCoord;
        gmsh::model::mesh::getElementProperties(elementTypes[t], name, dim, order,
                                                numNodes, localNodeCoord, numPrimaryNodes);
        const auto &connectivity = elementNodeTags[t];
        const std::size_t count = elementTags[t].size();
        numElements += count;
        std::cout << "  type " << elementTypes[t] << " (" << name << ") : "
                  << count << " elements\n";

        std::vector<std::size_t> nodes(numNodes);
        for (std::size_t e = 0; e < count; ++e) {
            for (int n = 0; n < numNodes; ++n)
                nodes[n] = tagToIndex.at(connectivity[e * numNodes + n]);

            for (const auto a : nodes)
                for (const auto b : nodes)
                    if (b > a)
                        edges.emplace_back(a, b);
        }
    }
    std::cout << "Elements : " << numElements << '\n';

    const std::size_t numPairs = edges.size();
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    std::cout << "Edges : " << edges.size() << " (" << numPairs - edges.size()
              << " duplicates removed)\n";

    return Graph(edges.begin(), edges.end(), nodeTags.size());
}

}  // namespace

// All data is read through the gmsh API, so the file format (msh 2.2, 4.x,
// ascii or binary, ...) does not matter.
Renumbering reverseCuthillMcKee()
{
    Renumbering r;
    r.nodeTags = sortedNodeTags();
    const std::size_t numNodes = r.nodeTags.size();
    std::cout << "Nodes : " << numNodes << '\n';
    if (numNodes == 0)
        throw std::runtime_error("no nodes in the mesh");

    Graph graph = buildNodeGraph(r.nodeTags);
    const auto indexMap = boost::get(boost::vertex_index, graph);
    r.bandwidthBefore = boost::bandwidth(graph);

    std::vector<Vertex> ordering(numNodes);
    boost::cuthill_mckee_ordering(graph, ordering.rbegin(),
                                  boost::get(boost::vertex_color, graph),
                                  boost::make_degree_map(graph));

    r.newIndex.resize(numNodes);
    r.oldIndex.resize(numNodes);
    for (std::size_t position = 0; position < numNodes; ++position) {
        const std::size_t node = indexMap[ordering[position]];
        r.newIndex[node] = position;
        r.oldIndex[position] = node;
    }

    r.bandwidthAfter = boost::bandwidth(
        graph, boost::make_iterator_property_map(r.newIndex.data(), indexMap, r.newIndex[0]));
    return r;
}

}  // namespace cmk
