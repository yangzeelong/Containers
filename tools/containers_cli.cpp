// Copyright (c) 2026 yangzeelong
//
// containers_cli - validate a manifest and print its launch order.
//
//   containers_cli <manifest.yaml> [--json]
//
// Exit codes: 0 valid, 1 invalid manifest, 2 usage/IO error.
#include "containers/engine.hpp"

#include <cstdio>
#include <iostream>
#include <string>

namespace {

const char* severity_name(containers::IssueSeverity s) {
    return s == containers::IssueSeverity::Error ? "ERROR" : "WARN ";
}

void print_issue(const containers::Issue& i) {
    std::fprintf(stderr, "%s %s: %s\n", severity_name(i.severity), i.path.c_str(),
                 i.message.c_str());
}

}    // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <manifest.yaml> [--json]\n", argv[0]);
        return 2;
    }
    bool json = false;
    std::string path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json")
            json = true;
        else
            path = arg;
    }
    if (path.empty()) {
        std::fprintf(stderr, "usage: %s <manifest.yaml> [--json]\n", argv[0]);
        return 2;
    }

    std::vector<containers::Issue> issues;
    containers::Manifest m = containers::load_manifest_file(path, &issues);
    for (const auto& i : issues) print_issue(i);
    if (!containers::Manifest::is_ok(issues)) {
        std::fprintf(stderr, "%s: invalid manifest\n", path.c_str());
        return 1;
    }

    const auto inherit_issues = m.resolve_inheritance();
    for (const auto& i : inherit_issues) print_issue(i);
    if (!containers::Manifest::is_ok(inherit_issues)) {
        std::fprintf(stderr, "%s: unresolved inheritance\n", path.c_str());
        return 1;
    }

    std::vector<containers::Issue> order_issues;
    const auto order = m.launch_order(&order_issues);
    for (const auto& i : order_issues) print_issue(i);
    if (!containers::Manifest::is_ok(order_issues)) return 1;

    if (json) {
        std::cout << "{\"services\":[";
        bool first = true;
        for (const auto& name : order) {
            const containers::Service* s = m.find_service(name);
            std::cout << (first ? "" : ",") << "{\"name\":\"" << name << "\",\"image\":\""
                      << (s ? s->image : "") << "\"}";
            first = false;
        }
        std::cout << "]}\n";
    } else {
        std::printf("%s: %zu service(s), valid\n", path.c_str(), order.size());
        std::size_t step = 1;
        for (const auto& name : order) {
            const containers::Service* s = m.find_service(name);
            std::printf("  %2zu. %-16s %s\n", step++, name.c_str(), s ? s->image.c_str() : "");
        }
    }
    return 0;
}
