// Minimal example: load ../configs/stack.yaml, resolve inheritance, print order.
#include "containers/engine.hpp"

#include <cstdio>

int main(int /*argc*/, char** /*argv*/) {
    std::vector<containers::Issue> issues;
    containers::Manifest m = containers::load_manifest_file(
        std::string(CONTAINERS_SOURCE_DIR) + "/configs/stack.yaml", &issues);
    if (!containers::Manifest::is_ok(issues)) {
        std::fprintf(stderr, "manifest is invalid\n");
        return 1;
    }
    const auto inh = m.resolve_inheritance();
    if (!containers::Manifest::is_ok(inh)) return 1;

    for (const auto& name : m.launch_order()) {
        const containers::Service* s = m.find_service(name);
        std::printf("%-12s %-24s ports=%zu\n", name.c_str(), s->image.c_str(), s->ports.size());
    }
    return 0;
}
