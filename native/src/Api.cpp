// Gözle Programlama IDE - Graf / tip serileştirme (JSON) + C ABI
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "gpi/Api.h"
#include "gpi/Codegen.hpp"
#include "gpi/Json.hpp"
#include "gpi/NodeCatalog.hpp"
#include "gpi/Serialization.hpp"
#include "gpi/Toolchain.hpp"
#include "gpi/Types.hpp"

namespace gpi {

// ===========================================================================
//  Tip serileştirme
// ===========================================================================

namespace {

const char* kindName(TypeKind k) {
  switch (k) {
    case TypeKind::Unknown: return "Unknown";
    case TypeKind::Void: return "Void";
    case TypeKind::Bool: return "Bool";
    case TypeKind::Int8: return "Int8";
    case TypeKind::Int16: return "Int16";
    case TypeKind::Int: return "Int32";
    case TypeKind::Long: return "Int64";
    case TypeKind::Float: return "Float";
    case TypeKind::Double: return "Double";
    case TypeKind::Char: return "Char";
    case TypeKind::String: return "String";
    case TypeKind::Byte: return "Byte";
    case TypeKind::Object: return "Object";
    case TypeKind::Any: return "Any";
    case TypeKind::Struct: return "Struct";
    case TypeKind::Class: return "Class";
    case TypeKind::Enum: return "Enum";
    case TypeKind::Array: return "Array";
    case TypeKind::List: return "List";
    case TypeKind::Dictionary: return "Dictionary";
    case TypeKind::Pointer: return "Pointer";
    case TypeKind::Reference: return "Reference";
    case TypeKind::Delegate: return "Delegate";
    case TypeKind::Variant: return "Variant";
  }
  return "Unknown";
}

TypeKind kindFrom(std::string_view s) {
  static const std::pair<const char*, TypeKind> table[] = {
      {"Unknown", TypeKind::Unknown}, {"Void", TypeKind::Void},   {"Bool", TypeKind::Bool},
      {"Int8", TypeKind::Int8},       {"Int16", TypeKind::Int16}, {"Int32", TypeKind::Int},
      {"Int64", TypeKind::Long},      {"Float", TypeKind::Float}, {"Double", TypeKind::Double},
      {"Char", TypeKind::Char},       {"String", TypeKind::String}, {"Byte", TypeKind::Byte},
      {"Object", TypeKind::Object},   {"Any", TypeKind::Any},     {"Struct", TypeKind::Struct},
      {"Class", TypeKind::Class},     {"Enum", TypeKind::Enum},   {"Array", TypeKind::Array},
      {"List", TypeKind::List},       {"Dictionary", TypeKind::Dictionary},
      {"Pointer", TypeKind::Pointer}, {"Reference", TypeKind::Reference},
      {"Delegate", TypeKind::Delegate}, {"Variant", TypeKind::Variant}};
  for (const auto& [n, k] : table) {
    if (s == n) return k;
  }
  return TypeKind::Unknown;
}

}  // namespace

using json::Value;

Value typeToJson(const TypeRef& t) {
  Value v = Value::object();
  v["kind"] = std::string(kindName(t.kind));
  v["name"] = t.name;
  v["display"] = std::string(toString(t.kind));
  v["nullable"] = t.nullable;
  if (!t.args.empty()) {
    Value arr = Value::array();
    for (const auto& a : t.args) arr.push(typeToJson(a));
    v["args"] = std::move(arr);
  }
  TypeSystem ts;
  const TypeNames& n = ts.names(t);
  v["csharp"] = n.csharp;
  v["cpp"] = n.cpp;
  return v;
}

TypeRef typeFromJson(const Value& v) {
  TypeRef t;
  t.kind = kindFrom(v["kind"].asString("Unknown"));
  t.name = v["name"].asString();
  t.nullable = v["nullable"].asBool(false);
  for (const auto& a : v["args"].items()) t.args.push_back(typeFromJson(a));
  if (t.name.empty() && t.args.empty()) t = TypeRef(t.kind);
  return t;
}

Value portToJson(const PortDef& p) {
  Value v = Value::object();
  v["name"] = p.name;
  v["displayName"] = p.displayName;
  v["direction"] = p.direction == PortDirection::Input ? "input" : "output";
  v["kind"] = p.kind == PortKind::Exec ? "exec" : "value";
  v["type"] = typeToJson(p.type);
  v["default"] = p.defaultLiteral;
  v["description"] = p.description;
  v["advanced"] = p.advanced;
  v["order"] = p.order;
  return v;
}

PortDef portFromJson(const Value& v) {
  PortDef p;
  p.name = v["name"].asString();
  p.displayName = v["displayName"].asString(p.name);
  p.direction = v["direction"].asString("input") == "output" ? PortDirection::Output
                                                             : PortDirection::Input;
  p.kind = v["kind"].asString("value") == "exec" ? PortKind::Exec : PortKind::Value;
  p.type = typeFromJson(v["type"]);
  p.defaultLiteral = v["default"].asString();
  p.description = v["description"].asString();
  p.advanced = v["advanced"].asBool(false);
  p.order = static_cast<int>(v["order"].asInt(0));
  return p;
}

Value nodeTypeToJson(const NodeType& t) {
  Value v = Value::object();
  v["id"] = t.id;
  v["displayName"] = t.displayName;
  v["category"] = t.category;
  v["subcategory"] = t.subcategory;
  v["description"] = t.description;
  v["icon"] = t.icon;
  v["accentColor"] = t.accentColor;
  v["library"] = t.library;
  v["pure"] = t.pure;
  v["isFlowControl"] = t.isFlowControl;
  v["deprecated"] = t.deprecated;
  v["cppHeader"] = t.cppHeader;

  Value ins = Value::array(), outs = Value::array();
  for (const auto& p : t.inputs) ins.push(portToJson(p));
  for (const auto& p : t.outputs) outs.push(portToJson(p));
  v["inputs"] = std::move(ins);
  v["outputs"] = std::move(outs);

  Value props = Value::object();
  for (const auto& [k, val] : t.properties) props[k] = val;
  v["properties"] = std::move(props);

  Value usings = Value::array();
  for (const auto& u : t.csUsings) usings.push(u);
  v["csUsings"] = std::move(usings);
  return v;
}

Value graphToJson(const Graph& g) {
  Value v = Value::object();
  v["name"] = g.name;

  Value nodes = Value::array();
  for (const auto& n : g.nodes) {
    Value nv = Value::object();
    nv["id"] = n.id;
    nv["type"] = n.typeId;
    nv["x"] = n.x;
    nv["y"] = n.y;
    nv["comment"] = n.comment;
    nv["collapsed"] = n.collapsed;
    nv["bypassed"] = n.bypassed;
    Value lits = Value::object();
    for (const auto& [k, val] : n.literals) lits[k] = val;
    nv["literals"] = std::move(lits);
    Value props = Value::object();
    for (const auto& [k, val] : n.properties) props[k] = val;
    nv["properties"] = std::move(props);
    nodes.push(std::move(nv));
  }
  v["nodes"] = std::move(nodes);

  Value edges = Value::array();
  for (const auto& e : g.edges) {
    Value ev = Value::object();
    Value from = Value::object();
    from["node"] = e.from.node;
    from["port"] = e.from.port;
    Value to = Value::object();
    to["node"] = e.to.node;
    to["port"] = e.to.port;
    ev["from"] = std::move(from);
    ev["to"] = std::move(to);
    edges.push(std::move(ev));
  }
  v["edges"] = std::move(edges);

  Value entries = Value::array();
  for (const auto& e : g.entryNodes) entries.push(e);
  v["entryNodes"] = std::move(entries);

  Value vars = Value::object();
  for (const auto& [k, val] : g.variables) vars[k] = val;
  v["variables"] = std::move(vars);

  Value subs = Value::object();
  for (const auto& [id, sg] : g.subgraphs) {
    Value sv = Value::object();
    sv["id"] = sg.id;
    sv["name"] = sg.name;
    sv["description"] = sg.description;
    Value si = Value::array(), so = Value::array();
    for (const auto& p : sg.inputs) si.push(portToJson(p));
    for (const auto& p : sg.outputs) so.push(portToJson(p));
    sv["inputs"] = std::move(si);
    sv["outputs"] = std::move(so);
    subs[id] = std::move(sv);
  }
  v["subgraphs"] = std::move(subs);
  return v;
}

bool graphFromJson(const Value& v, Graph& g, std::string* error) {
  g = Graph();
  g.name = v["name"].asString("AnaProgram");
  for (const auto& nv : v["nodes"].items()) {
    NodeInstance n;
    n.id = nv["id"].asString();
    n.typeId = nv["type"].asString();
    n.x = nv["x"].asDouble(0);
    n.y = nv["y"].asDouble(0);
    n.comment = nv["comment"].asString();
    n.collapsed = nv["collapsed"].asBool(false);
    n.bypassed = nv["bypassed"].asBool(false);
    for (const auto& [k, val] : nv["literals"].fields()) n.literals[k] = val.asString();
    for (const auto& [k, val] : nv["properties"].fields()) n.properties[k] = val.asString();
    if (n.id.empty() || n.typeId.empty()) {
      if (error) *error = "Kimliği veya tipi eksik düğüm var.";
      return false;
    }
    g.nodes.push_back(std::move(n));
  }
  for (const auto& ev : v["edges"].items()) {
    Edge e;
    e.from.node = ev["from"]["node"].asString();
    e.from.port = ev["from"]["port"].asString();
    e.to.node = ev["to"]["node"].asString();
    e.to.port = ev["to"]["port"].asString();
    if (e.from.node.empty() || e.to.node.empty()) continue;
    g.edges.push_back(std::move(e));
  }
  for (const auto& e : v["entryNodes"].items()) g.entryNodes.push_back(e.asString());
  for (const auto& [k, val] : v["variables"].fields()) g.variables[k] = val.asString();
  for (const auto& [id, sv] : v["subgraphs"].fields()) {
    SubGraph sg;
    sg.id = id;
    sg.name = sv["name"].asString(id);
    sg.description = sv["description"].asString();
    for (const auto& p : sv["inputs"].items()) sg.inputs.push_back(portFromJson(p));
    for (const auto& p : sv["outputs"].items()) sg.outputs.push_back(portFromJson(p));
    g.subgraphs[id] = std::move(sg);
  }
  return true;
}

ProjectOptions projectOptionsFromJson(const Value& v) {
  ProjectOptions o;
  o.lang = v["lang"].asString("csharp") == "cpp" ? TargetLang::Cpp : TargetLang::CSharp;
  o.appName = v["appName"].asString("Program");
  o.rootNamespace = v["rootNamespace"].asString(o.appName);
  o.assemblyName = v["assemblyName"].asString();
  o.outputType = v["outputType"].asString("Exe");
  o.cxxStandard = v["cxxStandard"].asString("20");
  o.includeComments = v["includeComments"].asBool(true);
  o.emitUsings = v["emitUsings"].asBool(true);
  o.inlinePureNodes = v["inlinePureNodes"].asBool(true);
  o.wrapInClass = v["wrapInClass"].asBool(true);
  o.framework = v["framework"].asString("net9.0");
  o.interopEnabled = v["interopEnabled"].asBool(false);
  o.interopNamespace = v["interopNamespace"].asString("Gpi.Interop");
  o.interopDllName = v["interopDllName"].asString("gpi_native");
  for (const auto& p : v["packages"].items()) {
    PackageRef r;
    r.id = p["id"].asString();
    r.version = p["version"].asString();
    if (!r.id.empty()) o.packages.push_back(std::move(r));
  }
  for (const auto& f : v["extraSourceFiles"].items()) o.extraSourceFiles.push_back(f.asString());
  for (const auto& l : v["libs"].items()) o.libs.push_back(l.asString());
  return o;
}

}  // namespace gpi

// ===========================================================================
//  C ABI
// ===========================================================================

namespace {

thread_local std::string g_lastError;
gp_log_fn g_logger = nullptr;
void* g_loggerUser = nullptr;

void logAt(int level, const std::string& msg) {
  if (g_logger) g_logger(level, msg.c_str(), g_loggerUser);
}

char* dupString(std::string_view s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  if (!p) return nullptr;
  std::memcpy(p, s.data(), s.size());
  p[s.size()] = '\0';
  return p;
}

char* toJsonString(const gpi::json::Value& v, int indent = -1) {
  return dupString(v.dump(indent));
}

gpi::NodeCatalog& catalog() {
  static gpi::NodeCatalog c;
  return c;
}

}  // namespace

extern "C" {

void GPI_CALL gp_free(char* ptr) { std::free(ptr); }

const char* GPI_CALL gp_version(void) { return "1.0.0"; }

const char* GPI_CALL gp_last_error(void) { return g_lastError.c_str(); }

void GPI_CALL gp_set_logger(gp_log_fn fn, void* user_data) {
  g_logger = fn;
  g_loggerUser = user_data;
}

char* GPI_CALL gp_graph_from_json(const char* json) {
  g_lastError.clear();
  if (!json) return nullptr;
  gpi::json::Value v = gpi::json::Value::parse(json, &g_lastError);
  if (!g_lastError.empty()) return nullptr;
  gpi::Graph g;
  if (!gpi::graphFromJson(v, g, &g_lastError)) return nullptr;
  return toJsonString(gpi::graphToJson(g));
}

char* GPI_CALL gp_project_options_from_json(const char* json) {
  g_lastError.clear();
  if (!json) return nullptr;
  gpi::json::Value v = gpi::json::Value::parse(json, &g_lastError);
  if (!g_lastError.empty()) return nullptr;
  return toJsonString(v);
}

char* GPI_CALL gp_catalog_json(void) {
  gpi::json::Value arr = gpi::json::Value::array();
  for (const auto& [id, t] : catalog().all()) arr.push(gpi::nodeTypeToJson(t));
  return toJsonString(arr);
}

char* GPI_CALL gp_catalog_categories_json(void) {
  gpi::json::Value arr = gpi::json::Value::array();
  for (const auto& c : catalog().categories()) {
    gpi::json::Value v = gpi::json::Value::object();
    v["id"] = c.id;
    v["displayName"] = c.displayName;
    v["icon"] = c.icon;
    arr.push(std::move(v));
  }
  return toJsonString(arr);
}

char* GPI_CALL gp_catalog_type_json(const char* typeId) {
  if (!typeId) return nullptr;
  const gpi::NodeType* t = catalog().find(typeId);
  if (!t) return nullptr;
  return toJsonString(gpi::nodeTypeToJson(*t));
}

char* GPI_CALL gp_catalog_search(const char* query, const char* library) {
  gpi::json::Value arr = gpi::json::Value::array();
  for (const auto& id : catalog().search(query ? query : "", library ? library : "")) {
    const gpi::NodeType* t = catalog().find(id);
    if (t) arr.push(gpi::nodeTypeToJson(*t));
  }
  return toJsonString(arr);
}

char* GPI_CALL gp_type_resolve(const char* typeJson) {
  if (!typeJson) return nullptr;
  gpi::json::Value v = gpi::json::Value::parse(typeJson);
  const gpi::TypeRef t = gpi::typeFromJson(v);
  return toJsonString(gpi::typeToJson(t));
}

int GPI_CALL gp_type_assignable(const char* fromJson, const char* toJson) {
  if (!fromJson || !toJson) return 0;
  gpi::json::Value a = gpi::json::Value::parse(fromJson);
  gpi::json::Value b = gpi::json::Value::parse(toJson);
  gpi::TypeSystem ts;
  return ts.assignable(gpi::typeFromJson(a), gpi::typeFromJson(b)) ? 1 : 0;
}

char* GPI_CALL gp_graph_analyze(const char* graphJson) {
  g_lastError.clear();
  if (!graphJson) return nullptr;
  gpi::json::Value v = gpi::json::Value::parse(graphJson, &g_lastError);
  if (!g_lastError.empty()) return nullptr;
  gpi::Graph g;
  if (!gpi::graphFromJson(v, g, &g_lastError)) return nullptr;

  gpi::AnalysisResult a = gpi::analyze(g, catalog());
  gpi::json::Value out = gpi::json::Value::object();
  out["ok"] = a.ok;
  gpi::json::Value errs = gpi::json::Value::array();
  for (const auto& e : a.errors) errs.push(e);
  gpi::json::Value warns = gpi::json::Value::array();
  for (const auto& w : a.warnings) warns.push(w);
  gpi::json::Value ord = gpi::json::Value::array();
  for (const auto& id : a.executionOrder) ord.push(id);
  out["errors"] = std::move(errs);
  out["warnings"] = std::move(warns);
  out["order"] = std::move(ord);
  return toJsonString(out);
}

char* GPI_CALL gp_generate_code(const char* graphJson, const char* optionsJson) {
  g_lastError.clear();
  if (!graphJson || !optionsJson) return nullptr;
  gpi::json::Value gv = gpi::json::Value::parse(graphJson, &g_lastError);
  if (!g_lastError.empty()) return nullptr;
  gpi::json::Value ov = gpi::json::Value::parse(optionsJson, &g_lastError);
  if (!g_lastError.empty()) return nullptr;

  gpi::Graph g;
  if (!gpi::graphFromJson(gv, g, &g_lastError)) return nullptr;
  const gpi::ProjectOptions opts = gpi::projectOptionsFromJson(ov);

  gpi::CodeGenerator gen(catalog());
  gpi::GenResult r = gen.generate(g, opts);

  gpi::json::Value out = gpi::json::Value::object();
  out["ok"] = r.ok;
  out["code"] = r.code;
  out["entryFunction"] = r.entryFunction;
  gpi::json::Value errs = gpi::json::Value::array();
  for (const auto& e : r.errors) errs.push(e);
  gpi::json::Value warns = gpi::json::Value::array();
  for (const auto& w : r.warnings) warns.push(w);
  gpi::json::Value usings = gpi::json::Value::array();
  for (const auto& u : r.requiredUsings) usings.push(u);
  out["errors"] = std::move(errs);
  out["warnings"] = std::move(warns);
  out["usings"] = std::move(usings);
  return toJsonString(out);
}

char* GPI_CALL gp_generate_project(const char* optionsJson) {
  g_lastError.clear();
  if (!optionsJson) return nullptr;
  gpi::json::Value ov = gpi::json::Value::parse(optionsJson, &g_lastError);
  if (!g_lastError.empty()) return nullptr;
  const gpi::ProjectOptions opts = gpi::projectOptionsFromJson(ov);
  gpi::ProjectGenerator pg;
  gpi::ProjectScaffold sc = opts.interopEnabled ? pg.scaffoldInterop(opts) : pg.scaffold(opts);

  gpi::json::Value out = gpi::json::Value::object();
  gpi::json::Value files = gpi::json::Value::array();
  for (const auto& f : sc.files) {
    gpi::json::Value fv = gpi::json::Value::object();
    fv["path"] = f.relativePath;
    fv["content"] = f.content;
    files.push(std::move(fv));
  }
  gpi::json::Value notes = gpi::json::Value::array();
  for (const auto& n : sc.notes) notes.push(n);
  out["files"] = std::move(files);
  out["notes"] = std::move(notes);
  return toJsonString(out);
}

char* GPI_CALL gp_generate_function(const char* graphJson, const char* subgraphJson,
                                     const char* optionsJson, const char* fnName) {
  g_lastError.clear();
  if (!graphJson || !optionsJson) return nullptr;
  gpi::json::Value gv = gpi::json::Value::parse(graphJson, &g_lastError);
  gpi::json::Value ov = gpi::json::Value::parse(optionsJson, &g_lastError);
  gpi::Graph g;
  gpi::SubGraph sg;
  if (subgraphJson) {
    gpi::json::Value sv = gpi::json::Value::parse(subgraphJson, &g_lastError);
    sg.id = sv["id"].asString();
    sg.name = sv["name"].asString("Fonksiyon");
    for (const auto& p : sv["inputs"].items()) sg.inputs.push_back(gpi::portFromJson(p));
    for (const auto& p : sv["outputs"].items()) sg.outputs.push_back(gpi::portFromJson(p));
  }
  if (!gpi::graphFromJson(gv, g, &g_lastError)) return nullptr;
  gpi::CodeGenerator gen(catalog());
  gpi::GenResult r = gen.generateFunction(g, sg, gpi::projectOptionsFromJson(ov),
                                          fnName ? fnName : "Fonksiyon");
  gpi::json::Value out = gpi::json::Value::object();
  out["ok"] = r.ok;
  out["code"] = r.code;
  return toJsonString(out);
}

namespace {

gpi::json::Value toolchainToJson(const gpi::ToolchainReport& r) {
  gpi::json::Value out = gpi::json::Value::object();
  gpi::json::Value tools = gpi::json::Value::array();
  for (const auto& t : r.tools) {
    gpi::json::Value v = gpi::json::Value::object();
    v["id"] = t.id;
    v["displayName"] = t.displayName;
    v["family"] = t.family;
    v["exePath"] = t.exePath;
    v["version"] = t.version;
    v["canBuildCSharp"] = t.canBuildCSharp;
    v["canBuildCpp"] = t.canBuildCpp;
    v["ready"] = t.ready;
    v["diagnostic"] = t.diagnostic;
    v["sdkVersion"] = t.sdkVersion;
    v["vcvars"] = t.vcvars;
    tools.push(std::move(v));
  }
  out["tools"] = std::move(tools);

  gpi::json::Value bs = gpi::json::Value::array();
  for (const auto& t : r.buildSystems) {
    gpi::json::Value v = gpi::json::Value::object();
    v["id"] = t.id;
    v["displayName"] = t.displayName;
    v["exePath"] = t.exePath;
    v["version"] = t.version;
    bs.push(std::move(v));
  }
  out["buildSystems"] = std::move(bs);

  out["preferredCSharp"] = r.preferredCSharp;
  out["preferredCpp"] = r.preferredCpp;
  out["dotnetSdkVersion"] = r.dotnetSdkVersion;
  out["windowsSdkVersion"] = r.windowsSdkVersion;
  out["nugetCachePath"] = r.nugetCachePath;
  out["vcpkgRoot"] = r.vcpkgRoot;
  gpi::json::Value notes = gpi::json::Value::array();
  for (const auto& n : r.notes) notes.push(n);
  gpi::json::Value errs = gpi::json::Value::array();
  for (const auto& e : r.errors) errs.push(e);
  out["notes"] = std::move(notes);
  out["errors"] = std::move(errs);
  out["canBuildCSharp"] = r.canBuildCSharp();
  out["canBuildCpp"] = r.canBuildCpp();
  return out;
}

gpi::json::Value buildResultToJson(const gpi::BuildResult& r) {
  gpi::json::Value out = gpi::json::Value::object();
  out["launched"] = r.launched;
  out["exitCode"] = r.exitCode;
  out["success"] = r.success;
  out["cancelled"] = r.cancelled;
  out["output"] = r.output;
  out["commandLine"] = r.commandLine;
  out["durationMs"] = r.durationMs;
  out["producedArtifact"] = r.producedArtifact;
  gpi::json::Value log = gpi::json::Value::array();
  for (const auto& l : r.log) log.push(l);
  out["log"] = std::move(log);
  gpi::json::Value diags = gpi::json::Value::array();
  for (const auto& d : r.diagnostics) {
    gpi::json::Value dv = gpi::json::Value::object();
    dv["severity"] = d.severity == gpi::Severity::Error   ? "error"
                     : d.severity == gpi::Severity::Warning ? "warning"
                                                             : "note";
    dv["file"] = d.file;
    dv["line"] = d.line;
    dv["column"] = d.column;
    dv["code"] = d.code;
    dv["message"] = d.message;
    dv["project"] = d.project;
    diags.push(std::move(dv));
  }
  out["diagnostics"] = std::move(diags);
  return out;
}

}  // namespace

char* GPI_CALL gp_probe_toolchain(void) {
  gpi::ToolchainDiscovery d;
  const gpi::ToolchainReport r = d.probe();
  return toJsonString(toolchainToJson(r), 2);
}

char* GPI_CALL gp_plan_build(const char* optionsJson) {
  g_lastError.clear();
  if (!optionsJson) return nullptr;
  gpi::json::Value ov = gpi::json::Value::parse(optionsJson, &g_lastError);
  if (!g_lastError.empty()) return nullptr;

  const gpi::ProjectOptions o = gpi::projectOptionsFromJson(ov);
  gpi::ToolchainDiscovery d;
  const gpi::ToolchainReport tc = d.probe();
  gpi::BuildPlanner bp;

  const std::string dir = ov["projectDir"].asString();
  gpi::BuildPlan plan;
  if (o.lang == gpi::TargetLang::CSharp) {
    plan = bp.planCSharp(tc, dir, ov["projectFile"].asString("Program.csproj"), o.lang);
  } else {
    // C++: önce MSBuild + vcxproj, yoksa doğrudan cl.exe
    plan = bp.planCppMsvc(tc, dir);
    if (!plan.valid) {
      plan = bp.planCppDirect(tc, dir + "/native/main.cpp", dir + "/bin/GpiProgram.exe");
    }
  }

  gpi::json::Value out = gpi::json::Value::object();
  out["valid"] = plan.valid;
  out["error"] = plan.error;
  out["target"] = plan.request.target;
  gpi::json::Value cmd = gpi::json::Value::array();
  for (const auto& c : plan.request.command) cmd.push(c);
  out["command"] = std::move(cmd);
  out["workingDirectory"] = plan.request.workingDirectory;
  out["primaryFile"] = plan.primaryFile;
  out["artifactHint"] = plan.artifactHint;
  out["timeoutSeconds"] = plan.request.timeoutSeconds;
  return toJsonString(out, 2);
}

char* GPI_CALL gp_build(const char* planJson) {
  g_lastError.clear();
  if (!planJson) return nullptr;
  gpi::json::Value v = gpi::json::Value::parse(planJson, &g_lastError);
  if (!g_lastError.empty()) return nullptr;

  gpi::BuildRequest req;
  req.workingDirectory = v["workingDirectory"].asString();
  for (const auto& c : v["command"].items()) req.command.push_back(c.asString());
  req.timeoutSeconds = static_cast<int>(v["timeoutSeconds"].asInt(600));
  if (req.command.empty()) {
    g_lastError = "Derleme komutu boş.";
    return nullptr;
  }

  logAt(GP_LOG_INFO, "Derleme başlıyor: " + req.command[0]);
  gpi::BuildRunner runner;
  gpi::BuildResult r = runner.run(
      req, [](const std::string& line, bool) { logAt(GP_LOG_INFO, line); });
  logAt(GP_LOG_INFO, r.success ? "Derleme başarılı." : "Derleme başarısız.");
  return toJsonString(buildResultToJson(r), 2);
}

char* GPI_CALL gp_run_executable(const char* exePath, const char* argsJson,
                                 const char* workingDir, int timeoutSeconds) {
  g_lastError.clear();
  if (!exePath) return nullptr;
  gpi::BuildRequest req;
  req.workingDirectory = workingDir ? workingDir : "";
  req.command.push_back(exePath);
  if (argsJson) {
    gpi::json::Value args = gpi::json::Value::parse(argsJson);
    for (const auto& a : args.items()) req.command.push_back(a.asString());
  }
  req.timeoutSeconds = timeoutSeconds > 0 ? timeoutSeconds : 60;
  gpi::BuildRunner runner;
  gpi::BuildResult r = runner.run(req, [](const std::string& line, bool) {
    logAt(GP_LOG_INFO, line);
  });
  return toJsonString(buildResultToJson(r));
}

char* GPI_CALL gp_nuget_search(const char* query, int take, int skip) {
  g_lastError.clear();
  const std::string q = query ? query : "";
  gpi::BuildPlanner bp;
  const gpi::OutputSink sink = [](const std::string&, bool) {};
  gpi::BuildResult r = bp.runAndCaptureOutput(
      "dotnet", {"package", "search", q, "--take", std::to_string(take > 0 ? take : 25),
                 "--skip", std::to_string(skip > 0 ? skip : 0), "--format", "json"},
      "", 90, sink);
  return dupString(r.output);
}

int GPI_CALL gp_nuget_add_package(const char* projectDir, const char* projectFile,
                                  const char* packageId, const char* version) {
  g_lastError.clear();
  if (!projectDir || !packageId) return -1;
  std::vector<std::string> args{"package", "add", packageId};
  if (version && *version) args.push_back(version);
  args.push_back("--project");
  args.push_back(projectFile ? projectFile : "");
  gpi::BuildPlanner bp;
  gpi::OutputSink sink = [](const std::string& line, bool) { logAt(GP_LOG_INFO, line); };
  gpi::BuildResult r = bp.runAndCaptureOutput("dotnet", args, projectDir, 300, sink);
  return r.success ? 0 : (r.exitCode == 0 ? -2 : r.exitCode);
}

const char* GPI_CALL gp_vcpkg_root(void) {
  static std::string root;
  gpi::ToolchainDiscovery d;
  const gpi::ToolchainReport r = d.probe();
  root = r.vcpkgRoot;
  return root.empty() ? nullptr : root.c_str();
}

}  // extern "C"
