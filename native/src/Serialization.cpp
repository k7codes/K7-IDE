// Gözle Programlama IDE - Serileştirme yardımcıları
#include "gpi/Serialization.hpp"

namespace gpi {

NodeInstance makeNode(const NodeCatalog& cat, std::string id, const std::string& typeId, double x,
                      double y) {
  NodeInstance n;
  n.id = std::move(id);
  n.typeId = typeId;
  n.x = x;
  n.y = y;
  if (const NodeType* t = cat.find(typeId)) cat.applyDefaults(*t, n);
  return n;
}

Edge makeEdge(std::string fromNode, std::string fromPort, std::string toNode, std::string toPort) {
  Edge e;
  e.from = PortKey{std::move(fromNode), std::move(fromPort)};
  e.to = PortKey{std::move(toNode), std::move(toPort)};
  return e;
}

Graph buildStringDemo(const NodeCatalog& cat) {
  Graph g;
  g.name = "DizeDemosu";

  auto lit = [&](const std::string& id, const std::string& value, double x, double y) {
    g.nodes.push_back(makeNode(cat, id, "lit.text", x, y));
    g.findNode(id)->literals["Value"] = value;
  };

  // Tuval pan'i içeriğin sol üstüne sabitlenir; koordinatlar 0'dan başlar.
  lit("t0", "\"Merhaba, \"", 0, 0);
  lit("t1", "\"görsel dünya!\"", 0, 160);

  g.nodes.push_back(makeNode(cat, "c0", "str.concat", 240, 60));
  g.edges.push_back(makeEdge("t0", "Value", "c0", "A"));
  g.edges.push_back(makeEdge("t1", "Value", "c0", "B"));

  g.nodes.push_back(makeNode(cat, "u0", "str.upper", 470, 60));
  g.edges.push_back(makeEdge("c0", "Result", "u0", "Value"));

  g.nodes.push_back(makeNode(cat, "p0", "io.print", 700, 60));
  g.edges.push_back(makeEdge("u0", "Result", "p0", "Value"));

  return g;
}

}  // namespace gpi
