// Gözle Programlama IDE - Serileştirme arayüzü
#pragma once

#include <string>

#include "gpi/Codegen.hpp"
#include "gpi/Graph.hpp"
#include "gpi/Json.hpp"
#include "gpi/NodeCatalog.hpp"
#include "gpi/Types.hpp"

namespace gpi {

json::Value typeToJson(const TypeRef& t);
TypeRef typeFromJson(const json::Value& v);
json::Value portToJson(const PortDef& p);
PortDef portFromJson(const json::Value& v);
json::Value nodeTypeToJson(const NodeType& t);
json::Value graphToJson(const Graph& g);
bool graphFromJson(const json::Value& v, Graph& g, std::string* error);
ProjectOptions projectOptionsFromJson(const json::Value& v);

// Yardımcılar
NodeInstance makeNode(const NodeCatalog& cat, std::string id, const std::string& typeId, double x,
                      double y);
Edge makeEdge(std::string fromNode, std::string fromPort, std::string toNode, std::string toPort);

// Örnek dize programı: iki metin → birleştir → büyük harfe çevir → yazdır.
// Uygulama "Örnek Program" eylemi ve gpcli "demo" komutu ortak kullanır.
Graph buildStringDemo(const NodeCatalog& cat);

}  // namespace gpi
