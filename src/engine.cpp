// Copyright (c) 2026 yangzeelong
#include "containers/engine.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace containers {
namespace {

using Env = std::vector<std::pair<std::string, std::string>>;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b   = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

bool is_absolute_unix_path(const std::string& p) { return !p.empty() && p.front() == '/'; }

void warn(std::vector<Issue>* issues, const std::string& path, const std::string& msg) {
    if (issues) issues->push_back(Issue{IssueSeverity::Warning, path, msg});
}

void error(std::vector<Issue>* issues, const std::string& path, const std::string& msg) {
    if (issues) issues->push_back(Issue{IssueSeverity::Error, path, msg});
}

/// Convert a YAML node that may be a mapping or a plain string into a list of
/// KEY=VALUE pairs. Mapping keys win nothing; order is document order.
bool read_env(const YAML::Node& node, Env* out, std::vector<Issue>* issues,
              const std::string& path) {
    if (!node || node.IsNull()) return true;
    if (node.IsMap()) {
        for (const auto& kv : node) {
            const std::string key = kv.first.as<std::string>();
            if (key.empty()) {
                error(issues, path, "environment key must not be empty");
                continue;
            }
            if (key.find('=') != std::string::npos) {
                error(issues, path + "." + key, "environment key must not contain '='");
                continue;
            }
            out->emplace_back(key, kv.second.IsNull() ? "" : kv.second.as<std::string>());
        }
        return true;
    }
    if (node.IsSequence()) {
        for (std::size_t i = 0; i < node.size(); ++i) {
            const std::string entry = node[i].as<std::string>();
            const auto eq           = entry.find('=');
            if (eq == std::string::npos || eq == 0) {
                error(issues, path + "[" + std::to_string(i) + "]",
                      "expected KEY=VALUE, got '" + entry + "'");
                continue;
            }
            out->emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
        }
        return true;
    }
    error(issues, path, "environment must be a mapping or a list of KEY=VALUE");
    return false;
}

/// Ports accept the short "HOST:CONTAINER[/proto]" form, a bare container port,
/// or a mapping {container, host, protocol}.
bool read_port_node(const YAML::Node& node, Port* out, std::vector<Issue>* issues,
                    const std::string& path) {
    if (node.IsScalar()) {
        const std::string text = node.as<std::string>();
        if (!parse_port(text, out)) {
            error(issues, path, "invalid port '" + text + "'");
            return false;
        }
        return true;
    }
    if (!node.IsMap()) {
        error(issues, path, "port must be a scalar or a mapping");
        return false;
    }
    Port p;
    if (!node["container"]) {
        error(issues, path, "port mapping requires 'container'");
        return false;
    }
    p.container = node["container"].as<int>();
    p.host      = node["host"] ? node["host"].as<int>() : 0;
    if (node["protocol"]) p.protocol = lower(node["protocol"].as<std::string>());
    if (p.container < 1 || p.container > 65535) {
        error(issues, path, "container port out of range: " + std::to_string(p.container));
        return false;
    }
    if (p.host < 0 || p.host > 65535) {
        error(issues, path, "host port out of range: " + std::to_string(p.host));
        return false;
    }
    if (p.protocol != "tcp" && p.protocol != "udp") {
        error(issues, path, "unsupported protocol '" + p.protocol + "'");
        return false;
    }
    *out = p;
    return true;
}

/// Mounts accept "volume:/path[:ro]" strings or {source, target, read_only}.
bool read_mount_node(const YAML::Node& node, Mount* out, std::vector<Issue>* issues,
                     const std::string& path) {
    Mount m;
    if (node.IsScalar()) {
        const std::string text = node.as<std::string>();
        // Split on ':' but keep a leading Windows drive letter intact.
        std::vector<std::string> parts;
        std::string cur;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if (c == ':' && !(i == 1 && std::isalpha(static_cast<unsigned char>(text[0])))) {
                parts.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        parts.push_back(cur);
        if (parts.size() < 2 || parts.size() > 3) {
            error(issues, path, "invalid mount '" + text + "'");
            return false;
        }
        m.source = trim(parts[0]);
        m.target = trim(parts[1]);
        if (parts.size() == 3) {
            const std::string opt = lower(trim(parts[2]));
            if (opt != "ro" && opt != "rw") {
                error(issues, path, "unsupported mount option '" + parts[2] + "'");
                return false;
            }
            m.read_only = (opt == "ro");
        }
    } else if (node.IsMap()) {
        if (!node["source"] || !node["target"]) {
            error(issues, path, "mount mapping requires 'source' and 'target'");
            return false;
        }
        m.source    = node["source"].as<std::string>();
        m.target    = node["target"].as<std::string>();
        m.read_only = node["read_only"] ? node["read_only"].as<bool>() : false;
    } else {
        error(issues, path, "mount must be a scalar or a mapping");
        return false;
    }
    if (m.source.empty()) {
        error(issues, path, "mount source must not be empty");
        return false;
    }
    if (!is_absolute_unix_path(m.target)) {
        error(issues, path, "mount target must be an absolute path, got '" + m.target + "'");
        return false;
    }
    *out = m;
    return true;
}

std::vector<std::string> read_string_list(const YAML::Node& node, std::vector<Issue>* issues,
                                          const std::string& path) {
    std::vector<std::string> out;
    if (!node || node.IsNull()) return out;
    if (node.IsScalar()) {
        error(issues, path, "expected a list, got a scalar");
        return out;
    }
    for (const auto& item : node) {
        const std::string v = trim(item.as<std::string>());
        if (!v.empty()) out.push_back(v);
    }
    return out;
}

void read_health(const YAML::Node& node, HealthCheck* h, std::vector<Issue>* issues,
                 const std::string& path) {
    h->enabled           = true;
    const YAML::Node cmd = node["test"] ? node["test"] : node["command"];
    if (cmd) {
        if (cmd.IsSequence()) {
            std::ostringstream os;
            bool first = true;
            for (const auto& part : cmd) {
                if (!first) os << ' ';
                os << part.as<std::string>();
                first = false;
            }
            h->command = os.str();
        } else {
            h->command = cmd.as<std::string>();
        }
    }
    if (h->command.empty()) error(issues, path, "healthcheck requires 'test' or 'command'");
    auto read_int = [&](const char* key, int* field, int min_value) {
        if (!node[key]) return;
        const int v = node[key].as<int>();
        if (v < min_value) {
            error(issues, path,
                  std::string("'") + key + "' must be >= " + std::to_string(min_value));
            return;
        }
        *field = v;
    };
    read_int("interval", &h->interval_seconds, 1);
    read_int("timeout", &h->timeout_seconds, 1);
    read_int("retries", &h->retries, 1);
    read_int("start_period", &h->start_period_seconds, 0);
    if (h->timeout_seconds > h->interval_seconds)
        warn(issues, path, "timeout exceeds interval; checks will overlap");
}

void read_resources(const YAML::Node& node, ResourceLimit* r, std::vector<Issue>* issues,
                    const std::string& path) {
    if (node["cpus"]) {
        const double c = node["cpus"].as<double>();
        if (c <= 0.0) {
            error(issues, path + ".cpus", "cpus must be > 0");
        } else {
            r->cpus = c;
        }
    }
    if (node["memory"]) {
        const std::string m = trim(node["memory"].as<std::string>());
        if (m.empty()) {
            error(issues, path + ".memory", "memory must not be empty");
        } else {
            r->memory = m;
        }
    }
}

}    // namespace

std::string to_string(RestartPolicy p) {
    switch (p) {
        case RestartPolicy::No: return "no";
        case RestartPolicy::OnFailure: return "on-failure";
        case RestartPolicy::Always: return "always";
        case RestartPolicy::UnlessStopped: return "unless-stopped";
    }
    return "no";
}

bool parse_restart_policy(const std::string& text, RestartPolicy* out) {
    const std::string t = lower(trim(text));
    if (t == "no")
        *out = RestartPolicy::No;
    else if (t == "on-failure")
        *out = RestartPolicy::OnFailure;
    else if (t == "always")
        *out = RestartPolicy::Always;
    else if (t == "unless-stopped")
        *out = RestartPolicy::UnlessStopped;
    else
        return false;
    return true;
}

void Service::merge_from(const Service& parent) {
    if (image.empty()) image = parent.image;
    if (restart == RestartPolicy::No) restart = parent.restart;
    if (resources.cpus == 0.0) resources.cpus = parent.resources.cpus;
    if (resources.memory.empty()) resources.memory = parent.resources.memory;

    auto append_unique = [](std::vector<std::string>& dst, const std::vector<std::string>& src) {
        for (const auto& v : src)
            if (std::find(dst.begin(), dst.end(), v) == dst.end()) dst.push_back(v);
    };
    // Parent first, child second: child overrides at runtime because runtimes
    // apply the last duplicate value.
    std::vector<std::string> merged_deps = parent.depends_on;
    append_unique(merged_deps, depends_on);
    depends_on.swap(merged_deps);

    std::vector<std::string> merged_nets = parent.networks;
    append_unique(merged_nets, networks);
    networks.swap(merged_nets);

    std::vector<std::string> merged_cmd = parent.command;
    if (!command.empty()) merged_cmd = command;
    command.swap(merged_cmd);

    std::vector<Mount> merged_mounts = parent.mounts;
    for (const auto& m : mounts) {
        auto it = std::find_if(merged_mounts.begin(), merged_mounts.end(),
                               [&](const Mount& x) { return x.target == m.target; });
        if (it != merged_mounts.end())
            *it = m;
        else
            merged_mounts.push_back(m);
    }
    mounts.swap(merged_mounts);

    std::vector<Port> merged_ports = parent.ports;
    for (const auto& p : ports) {
        if (std::find(merged_ports.begin(), merged_ports.end(), p) == merged_ports.end())
            merged_ports.push_back(p);
    }
    ports.swap(merged_ports);

    Env merged_env = parent.env;
    for (const auto& kv : env) {
        bool replaced = false;
        for (auto& existing : merged_env) {
            if (existing.first == kv.first) {
                existing.second = kv.second;
                replaced        = true;
                break;
            }
        }
        if (!replaced) merged_env.push_back(kv);
    }
    env.swap(merged_env);

    if (health.enabled) {
        // Child overrides block-wise; unspecified keys fall back to the parent.
        HealthCheck merged = parent.health;
        if (!health.command.empty()) merged = health;
        health = merged;
    }

    for (const auto& l : parent.labels)
        if (std::find(labels.begin(), labels.end(), l) == labels.end()) labels.push_back(l);
}

bool Service::find_env(const std::string& key, std::string* value) const {
    for (const auto& kv : env) {
        if (kv.first == key) {
            if (value) *value = kv.second;
            return true;
        }
    }
    return false;
}

bool Manifest::add_service(Service service) {
    if (service.name.empty() || find_service(service.name)) return false;
    services_.push_back(std::move(service));
    return true;
}

Service* Manifest::find_service(const std::string& name) {
    for (auto& s : services_)
        if (s.name == name) return &s;
    return nullptr;
}

const Service* Manifest::find_service(const std::string& name) const {
    for (const auto& s : services_)
        if (s.name == name) return &s;
    return nullptr;
}

bool Manifest::is_ok(const std::vector<Issue>& issues) {
    for (const auto& i : issues)
        if (i.severity == IssueSeverity::Error) return false;
    return true;
}

std::vector<Issue> Manifest::validate() const {
    std::vector<Issue> issues;

    std::set<std::string> svc_names, net_names, vol_names;
    for (const auto& s : services_)
        if (!svc_names.insert(s.name).second)
            error(&issues, "services." + s.name, "duplicate service name");
    for (const auto& n : networks_)
        if (!net_names.insert(n.name).second)
            error(&issues, "networks." + n.name, "duplicate network name");
    for (const auto& v : volumes_)
        if (!vol_names.insert(v.name).second)
            error(&issues, "volumes." + v.name, "duplicate volume name");

    std::set<std::string> host_ports;
    for (const auto& s : services_) {
        const std::string base = "services." + s.name;
        if (s.name.empty()) error(&issues, base, "service name must not be empty");
        if (s.image.empty() && s.extends_from.empty())
            error(&issues, base + ".image", "service requires an image");
        if (!s.image.empty() && s.image.find(':') == std::string::npos)
            warn(&issues, base + ".image", "no tag pinned; 'latest' will be assumed");

        for (const auto& d : s.depends_on) {
            if (d == s.name)
                error(&issues, base + ".depends_on", "service depends on itself");
            else if (!svc_names.count(d))
                error(&issues, base + ".depends_on", "unknown service '" + d + "'");
        }
        for (const auto& n : s.networks)
            if (!net_names.count(n))
                error(&issues, base + ".networks", "unknown network '" + n + "'");
        // A child service may inherit its networks from the parent, so only warn
        // once inheritance has been resolved for services that stand alone.
        if (s.networks.empty() && s.extends_from.empty() && !networks_.empty())
            warn(&issues, base + ".networks", "no network attached; using default");

        for (std::size_t i = 0; i < s.ports.size(); ++i) {
            const auto& p           = s.ports[i];
            const std::string ppath = base + ".ports[" + std::to_string(i) + "]";
            if (p.host == 0) continue;
            const std::string key = p.protocol + "/" + std::to_string(p.host);
            if (!host_ports.insert(key).second)
                error(&issues, ppath,
                      "host port " + std::to_string(p.host) + "/" + p.protocol +
                          " already published by another service");
        }
        for (std::size_t i = 0; i < s.mounts.size(); ++i) {
            const auto& m        = s.mounts[i];
            const bool host_path = m.source.find('/') != std::string::npos ||
                                   m.source.find('\\') != std::string::npos ||
                                   m.source.find('.') == 0;
            if (!host_path && !vol_names.count(m.source))
                error(&issues, base + ".volumes[" + std::to_string(i) + "]",
                      "unknown volume '" + m.source + "'");
        }
        std::set<std::string> env_keys;
        for (const auto& kv : s.env) {
            const std::string lk = lower(kv.first);
            if (!env_keys.insert(lk).second)
                warn(&issues, base + ".environment", "duplicate key '" + kv.first + "'");
        }
    }
    return issues;
}

std::vector<Issue> Manifest::resolve_inheritance() {
    std::vector<Issue> issues;
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < services_.size(); ++i) index[services_[i].name] = i;

    // 0 = unvisited, 1 = in progress, 2 = done.
    std::vector<int> state(services_.size(), 0);
    std::vector<bool> resolved(services_.size(), false);

    // Iterative DFS so a long extends chain can't blow the stack.
    for (std::size_t root = 0; root < services_.size(); ++root) {
        if (state[root] == 2) continue;
        std::vector<std::size_t> stack;
        stack.push_back(root);
        while (!stack.empty()) {
            const std::size_t cur = stack.back();
            Service& svc          = services_[cur];
            if (state[cur] == 0) {
                state[cur]    = 1;
                const auto it = index.find(svc.extends_from);
                if (!svc.extends_from.empty()) {
                    if (it == index.end()) {
                        error(&issues, "services." + svc.name + ".extends",
                              "unknown parent service '" + svc.extends_from + "'");
                        state[cur] = 2;
                        stack.pop_back();
                        continue;
                    }
                    if (it->second == cur) {
                        error(&issues, "services." + svc.name + ".extends",
                              "service extends itself");
                        state[cur] = 2;
                        stack.pop_back();
                        continue;
                    }
                    if (state[it->second] == 1) {
                        error(&issues, "services." + svc.name + ".extends",
                              "cycle detected in extends chain");
                        state[cur] = 2;
                        stack.pop_back();
                        continue;
                    }
                    if (state[it->second] != 2) {
                        stack.push_back(it->second);
                        continue;
                    }
                    const Service parent = services_[it->second];
                    svc.merge_from(parent);
                    resolved[cur] = true;
                } else {
                    resolved[cur] = true;
                }
                if (svc.image.empty())
                    error(&issues, "services." + svc.name + ".image",
                          "no image after resolving extends");
                state[cur] = 2;
                stack.pop_back();
                continue;
            }
            // state == 1 and no work pushed: finish this node.
            state[cur] = 2;
            stack.pop_back();
        }
    }
    return issues;
}

std::vector<std::string> Manifest::launch_order(std::vector<Issue>* issues) const {
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < services_.size(); ++i) index[services_[i].name] = i;

    // Kahn's algorithm with a min-heap on declaration index, so services with
    // no ordering constraint keep their file order (stable output for diffing).
    std::vector<std::vector<std::size_t>> outs(services_.size());
    std::vector<std::size_t> indeg(services_.size(), 0);
    bool ok = true;

    for (std::size_t i = 0; i < services_.size(); ++i) {
        for (const auto& dep : services_[i].depends_on) {
            const auto it = index.find(dep);
            if (it == index.end()) {
                error(issues, "services." + services_[i].name + ".depends_on",
                      "unknown service '" + dep + "'");
                ok = false;
                continue;
            }
            if (it->second == i) continue;    // self-edge already reported by validate()
            outs[it->second].push_back(i);
            ++indeg[i];
        }
    }

    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready;
    for (std::size_t i = 0; i < services_.size(); ++i)
        if (indeg[i] == 0) ready.push(i);

    std::vector<std::string> order;
    while (!ready.empty()) {
        const std::size_t cur = ready.top();
        ready.pop();
        order.push_back(services_[cur].name);
        for (const std::size_t nxt : outs[cur])
            if (--indeg[nxt] == 0) ready.push(nxt);
    }

    if (ok && order.size() != services_.size()) {
        std::vector<std::string> stuck;
        for (std::size_t i = 0; i < services_.size(); ++i)
            if (indeg[i] > 0) stuck.push_back(services_[i].name);
        std::string joined;
        for (const auto& s : stuck) joined += (joined.empty() ? "" : ", ") + s;
        error(issues, "services", "dependency cycle among: " + joined);
        ok = false;
    }
    return ok ? order : std::vector<std::string>();
}

Manifest load_manifest_string(const std::string& text, std::vector<Issue>* issues) {
    Manifest m;
    YAML::Node root;
    try {
        root = YAML::Load(text);
    } catch (const YAML::Exception& e) {
        error(issues, "<document>", std::string("YAML parse error: ") + e.what());
        return m;
    }
    if (!root || root.IsNull()) {
        error(issues, "<document>", "document is empty");
        return m;
    }
    if (!root.IsMap()) {
        error(issues, "<document>", "top level must be a mapping");
        return m;
    }

    for (const auto& kv : root) {
        const std::string key = kv.first.as<std::string>();
        if (key == "version") {
            m.set_version(kv.second.as<std::string>());
        } else if (key != "services" && key != "networks" && key != "volumes") {
            warn(issues, key, "unknown top-level key ignored");
        }
    }

    if (const YAML::Node nets = root["networks"]) {
        if (!nets.IsMap()) {
            error(issues, "networks", "must be a mapping");
        } else {
            for (const auto& kv : nets) {
                Network n;
                n.name                 = kv.first.as<std::string>();
                const YAML::Node body  = kv.second;
                const std::string base = "networks." + n.name;
                if (body && body.IsMap()) {
                    if (body["internal"]) n.internal = body["internal"].as<bool>();
                    if (body["driver"]) n.driver = body["driver"].as<std::string>();
                } else if (body && !body.IsNull()) {
                    error(issues, base, "network must be null or a mapping");
                }
                m.networks().push_back(std::move(n));
            }
        }
    }

    if (const YAML::Node vols = root["volumes"]) {
        if (!vols.IsMap()) {
            error(issues, "volumes", "must be a mapping");
        } else {
            for (const auto& kv : vols) {
                Volume v;
                v.name                 = kv.first.as<std::string>();
                const YAML::Node body  = kv.second;
                const std::string base = "volumes." + v.name;
                if (body && body.IsMap()) {
                    if (body["driver"]) v.driver = body["driver"].as<std::string>();
                    if (body["read_only"]) v.read_only = body["read_only"].as<bool>();
                } else if (body && !body.IsNull()) {
                    error(issues, base, "volume must be null or a mapping");
                }
                m.volumes().push_back(std::move(v));
            }
        }
    }

    const YAML::Node svcs = root["services"];
    if (!svcs) {
        error(issues, "services", "manifest must declare 'services'");
        return m;
    }
    if (!svcs.IsMap()) {
        error(issues, "services", "must be a mapping of name -> service");
        return m;
    }

    for (const auto& kv : svcs) {
        Service s;
        s.name                 = kv.first.as<std::string>();
        const std::string base = "services." + s.name;
        const YAML::Node body  = kv.second;
        if (!body || !body.IsMap()) {
            error(issues, base, "service definition must be a mapping");
            continue;
        }

        if (body["image"]) s.image = trim(body["image"].as<std::string>());
        if (body["extends"]) s.extends_from = body["extends"].as<std::string>();
        if (body["depends_on"])
            s.depends_on = read_string_list(body["depends_on"], issues, base + ".depends_on");
        if (body["networks"])
            s.networks = read_string_list(body["networks"], issues, base + ".networks");
        if (body["labels"]) s.labels = read_string_list(body["labels"], issues, base + ".labels");

        if (const YAML::Node cmd = body["command"]) {
            if (cmd.IsSequence()) {
                for (const auto& part : cmd) s.command.push_back(part.as<std::string>());
            } else {
                s.command = split_command(cmd.as<std::string>());
            }
        }

        read_env(body["environment"], &s.env, issues, base + ".environment");

        if (const YAML::Node ports = body["ports"]) {
            if (!ports.IsSequence()) {
                error(issues, base + ".ports", "must be a list");
            } else {
                for (std::size_t i = 0; i < ports.size(); ++i) {
                    Port p;
                    if (read_port_node(ports[i], &p, issues,
                                       base + ".ports[" + std::to_string(i) + "]"))
                        s.ports.push_back(p);
                }
            }
        }

        if (const YAML::Node mounts = body["volumes"]) {
            if (!mounts.IsSequence()) {
                error(issues, base + ".volumes", "must be a list");
            } else {
                for (std::size_t i = 0; i < mounts.size(); ++i) {
                    Mount mt;
                    if (read_mount_node(mounts[i], &mt, issues,
                                        base + ".volumes[" + std::to_string(i) + "]"))
                        s.mounts.push_back(mt);
                }
            }
        }

        if (const YAML::Node hc = body["healthcheck"]) {
            if (hc.IsMap()) {
                read_health(hc, &s.health, issues, base + ".healthcheck");
            } else {
                error(issues, base + ".healthcheck", "must be a mapping");
            }
        }

        if (const YAML::Node res = body["resources"])
            read_resources(res, &s.resources, issues, base + ".resources");

        if (body["restart"]) {
            const std::string text = body["restart"].as<std::string>();
            if (!parse_restart_policy(text, &s.restart))
                error(issues, base + ".restart", "unsupported policy '" + text + "'");
        }

        if (!m.add_service(std::move(s))) error(issues, base, "duplicate or empty service name");
    }

    // Parsing only proves the document is well-formed. Run the structural checks
    // here so callers that just load a file get semantic errors too; inheritance
    // stays explicit because it mutates the model.
    for (const auto& issue : m.validate()) issues->push_back(issue);
    return m;
}

Manifest load_manifest_file(const std::string& path, std::vector<Issue>* issues) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error(issues, "<file>", "cannot open '" + path + "'");
        return Manifest{};
    }
    std::ostringstream os;
    os << in.rdbuf();
    return load_manifest_string(os.str(), issues);
}

bool parse_port(const std::string& text, Port* out) {
    std::string t = trim(text);
    if (t.empty()) return false;

    Port p;
    const auto slash = t.find('/');
    if (slash != std::string::npos) {
        p.protocol = lower(t.substr(slash + 1));
        t          = t.substr(0, slash);
        if (p.protocol != "tcp" && p.protocol != "udp") return false;
    }

    std::vector<std::string> parts;
    std::string cur;
    for (const char c : t) {
        if (c == ':') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    if (parts.size() > 3) return false;

    auto to_port = [](const std::string& s, int* v) {
        if (s.empty()) return false;
        for (const char c : s)
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        const long n = std::stol(s);
        if (n < 1 || n > 65535) return false;
        *v = static_cast<int>(n);
        return true;
    };

    if (parts.size() == 3) {
        // host_ip:host_port:container_port - the bind IP is not modelled.
        if (parts[0].empty()) return false;
        if (!to_port(parts[1], &p.host)) return false;
        if (!to_port(parts[2], &p.container)) return false;
    } else if (parts.size() == 2) {
        if (!to_port(parts[0], &p.host)) return false;
        if (!to_port(parts[1], &p.container)) return false;
    } else {
        if (!to_port(parts[0], &p.container)) return false;
    }
    *out = p;
    return true;
}

std::vector<std::string> split_command(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    char quote     = 0;
    bool escaped   = false;
    bool has_token = false;

    for (const char c : text) {
        if (escaped) {
            cur.push_back(c);
            escaped   = false;
            has_token = true;
            continue;
        }
        if (c == '\\' && quote != '\'') {
            escaped = true;
            continue;
        }
        if (quote) {
            if (c == quote)
                quote = 0;
            else
                cur.push_back(c);
            has_token = true;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote     = c;
            has_token = true;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (has_token) {
                out.push_back(cur);
                cur.clear();
                has_token = false;
            }
            continue;
        }
        cur.push_back(c);
        has_token = true;
    }
    if (escaped) cur.push_back('\\');
    if (has_token) out.push_back(cur);
    return out;
}

}    // namespace containers
