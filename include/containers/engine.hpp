// Copyright (c) 2026 yangzeelong
//
// Containers - a tiny, dependency-light model for describing and validating
// YAML container manifests (compose-style services, networks and volumes).
//
// The design goal is deliberately narrow: load a YAML document, validate it in
// one pass, resolve `extends` inheritance and produce a deterministic launch
// order. Nothing here talks to a container runtime; turning the resulting plan
// into running containers is somebody else's job.
#ifndef CONTAINERS_ENGINE_HPP
#define CONTAINERS_ENGINE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace containers {

/// A single published/exposed port. `protocol` is "tcp" or "udp".
struct Port {
    int host             = 0;    ///< 0 means "let the runtime pick" (exposed only).
    int container        = 0;    ///< Port inside the container; must be 1..65535.
    std::string protocol = "tcp";

    bool operator==(const Port& o) const {
        return host == o.host && container == o.container && protocol == o.protocol;
    }
};

/// A named volume mount inside a service.
struct Mount {
    std::string source;    ///< Named volume (resolved to a Volume) or host path.
    std::string target;    ///< Absolute path inside the container.
    bool read_only = false;

    bool operator==(const Mount& o) const {
        return source == o.source && target == o.target && read_only == o.read_only;
    }
};

/// Health check policy, expressed with the same keys compose uses.
struct HealthCheck {
    bool enabled = false;
    std::string command;    ///< Shell-free command string, split by the runtime.
    int interval_seconds     = 30;
    int timeout_seconds      = 5;
    int retries              = 3;
    int start_period_seconds = 0;

    bool operator==(const HealthCheck& o) const {
        return enabled == o.enabled && command == o.command &&
               interval_seconds == o.interval_seconds && timeout_seconds == o.timeout_seconds &&
               retries == o.retries && start_period_seconds == o.start_period_seconds;
    }
};

/// Multiplier applied when translating CPU shares into runtime limits.
struct ResourceLimit {
    double cpus = 0.0;     ///< 0 means unlimited. Fractional cores.
    std::string memory;    ///< e.g. "512Mi", empty = unlimited.

    bool operator==(const ResourceLimit& o) const { return cpus == o.cpus && memory == o.memory; }
};

/// Restart policy in compose vocabulary.
enum class RestartPolicy { No, OnFailure, Always, UnlessStopped };

std::string to_string(RestartPolicy p);
bool parse_restart_policy(const std::string& text, RestartPolicy* out);

/// One service (container) from a manifest.
struct Service {
    std::string name;
    std::string image;                   ///< Required unless the service only extends.
    std::string extends_from;            ///< Optional parent service name.
    std::vector<std::string> command;    ///< Overrides image CMD when non-empty.
    std::vector<std::string> depends_on;
    std::vector<std::string> networks;
    std::vector<Mount> mounts;
    std::vector<std::pair<std::string, std::string>> env;    ///< KEY=VALUE pairs.
    std::vector<Port> ports;
    HealthCheck health;
    ResourceLimit resources;
    RestartPolicy restart = RestartPolicy::No;
    std::vector<std::string> labels;

    /// Merge `parent` into this service. Explicit fields win; list fields are
    /// concatenated with parent entries first (compose 3.x semantics).
    void merge_from(const Service& parent);

    /// Case-insensitive environment lookup; false when the key is absent.
    bool find_env(const std::string& key, std::string* value) const;
};

struct Network {
    std::string name;
    bool internal      = false;
    std::string driver = "bridge";
};

struct Volume {
    std::string name;
    std::string driver = "local";
    bool read_only     = false;
};

enum class IssueSeverity { Error, Warning };

/// A validation problem with enough context to be actionable.
struct Issue {
    IssueSeverity severity = IssueSeverity::Error;
    std::string path;    ///< Dotted location, e.g. "services.web.ports[0]".
    std::string message;

    bool operator==(const Issue& o) const {
        return severity == o.severity && path == o.path && message == o.message;
    }
};

/// A fully parsed manifest. Services are stored in declaration order.
class Manifest {
public:
    Manifest() = default;

    const std::string& version() const { return version_; }
    void set_version(std::string v) { version_ = std::move(v); }

    std::vector<Service>& services() { return services_; }
    const std::vector<Service>& services() const { return services_; }

    std::vector<Network>& networks() { return networks_; }
    const std::vector<Network>& networks() const { return networks_; }

    std::vector<Volume>& volumes() { return volumes_; }
    const std::vector<Volume>& volumes() const { return volumes_; }

    /// Add a service; returns false (and leaves the manifest unchanged) when
    /// the name is empty or already taken.
    bool add_service(Service service);

    /// Pointer to a service by name, or nullptr.
    Service* find_service(const std::string& name);
    const Service* find_service(const std::string& name) const;

    /// Validate structure only: required fields, ranges, duplicate names and
    /// referential integrity. Does NOT resolve `extends`.
    std::vector<Issue> validate() const;

    /// Resolve every `extends_from` in place. Parents defined in the same
    /// manifest are merged into children. Cycles, unknown parents and an
    /// empty image after merging are reported as errors.
    std::vector<Issue> resolve_inheritance();

    /// Topological order over `depends_on`. Self-edges, unknown targets and
    /// cycles are errors; on error the returned order is empty.
    std::vector<std::string> launch_order(std::vector<Issue>* issues = nullptr) const;

    /// True when no issue of severity Error exists.
    static bool is_ok(const std::vector<Issue>& issues);

private:
    std::string version_ = "3.9";
    std::vector<Service> services_;
    std::vector<Network> networks_;
    std::vector<Volume> volumes_;
};

/// Load and validate a manifest from a YAML string. All structural problems are
/// collected in `issues`; on a parse failure the manifest is left empty and a
/// single error is reported with path "<document>".
Manifest load_manifest_string(const std::string& text, std::vector<Issue>* issues);

/// Same as above, reading the file first. A missing/unreadable file produces
/// one error with path "<file>".
Manifest load_manifest_file(const std::string& path, std::vector<Issue>* issues);

/// Normalize a YAML scalar port form ("8080:80", "127.0.0.1:5432:5432",
/// "5000/udp") into a Port. Returns false when the text is malformed or
/// out of range.
bool parse_port(const std::string& text, Port* out);

/// Split a command string respecting quotes: `sh -c "echo hi"` -> 3 tokens.
std::vector<std::string> split_command(const std::string& text);

}    // namespace containers

#endif    // CONTAINERS_ENGINE_HPP
