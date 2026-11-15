// Copyright (c) 2026 yangzeelong
//
// Tests for the manifest model. Kept beside the implementation (*Test.cpp) so
// CMake's suffix split folds them into `containers_tests` automatically.
#include "containers/engine.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using containers::Issue;
using containers::IssueSeverity;
using containers::Manifest;
using containers::Port;
using containers::RestartPolicy;

namespace {

bool has_error(const std::vector<Issue>& issues, const std::string& path_substr) {
    for (const auto& i : issues) {
        if (i.severity == IssueSeverity::Error && i.path.find(path_substr) != std::string::npos)
            return true;
    }
    return false;
}

bool has_error_anywhere(const std::vector<Issue>& issues, const std::string& substr) {
    for (const auto& i : issues) {
        if (i.severity != IssueSeverity::Error) continue;
        if (i.path.find(substr) != std::string::npos || i.message.find(substr) != std::string::npos)
            return true;
    }
    return false;
}

const char* kBasic = R"YAML(
version: "3.9"
x-note: supported-extension-key
services:
  db:
    image: postgres:16
    environment:
      - POSTGRES_PASSWORD=secret
    volumes:
      - pgdata:/var/lib/postgresql/data
    networks: [backend]
  api:
    image: my-api:1.2.3
    depends_on: [db]
    ports:
      - "8080:80"
      - "5000/udp"
    networks: [backend, frontend]
    restart: unless-stopped
networks:
  backend:
    internal: true
  frontend: {}
volumes:
  pgdata:
)YAML";

}    // namespace

TEST(EngineParse, BasicManifestLoads) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(kBasic, &issues);
    EXPECT_TRUE(Manifest::is_ok(issues));
    ASSERT_EQ(m.services().size(), 2u);
    EXPECT_EQ(m.version(), "3.9");
    ASSERT_EQ(m.networks().size(), 2u);
    EXPECT_TRUE(m.networks()[0].internal);
    EXPECT_EQ(m.volumes().size(), 1u);

    const containers::Service* api = m.find_service("api");
    ASSERT_NE(api, nullptr);
    EXPECT_EQ(api->restart, RestartPolicy::UnlessStopped);
    ASSERT_EQ(api->ports.size(), 2u);
    EXPECT_EQ(api->ports[0].host, 8080);
    EXPECT_EQ(api->ports[0].container, 80);
    EXPECT_EQ(api->ports[1].container, 5000);
    EXPECT_EQ(api->ports[1].protocol, "udp");
}

TEST(EngineParse, UnknownTopLevelKeyWarns) {
    std::vector<Issue> issues;
    containers::load_manifest_string(kBasic, &issues);
    bool warned = false;
    for (const auto& i : issues)
        if (i.severity == IssueSeverity::Warning && i.path == "x-note") warned = true;
    EXPECT_TRUE(warned);
}

TEST(EngineParse, MissingServicesIsError) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string("version: '3'\n", &issues);
    EXPECT_TRUE(m.services().empty());
    EXPECT_TRUE(has_error(issues, "services"));
}

TEST(EngineParse, MalformedYamlReportsParseError) {
    std::vector<Issue> issues;
    containers::load_manifest_string("services: [unclosed\n", &issues);
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().path, "<document>");
    EXPECT_NE(issues.front().message.find("YAML parse error"), std::string::npos);
}

TEST(EngineParse, EmptyDocumentIsError) {
    std::vector<Issue> issues;
    containers::load_manifest_string("", &issues);
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().message, "document is empty");
}

TEST(EngineValidation, UnknownDependencyAndVolume) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(
        "services:\n"
        "  web:\n"
        "    image: nginx:1.27\n"
        "    depends_on: [cache]\n"
        "    volumes: ['missing:/data']\n",
        &issues);
    EXPECT_TRUE(has_error(issues, "services.web.depends_on"));
    EXPECT_TRUE(has_error(issues, "services.web.volumes[0]"));
}

TEST(EngineValidation, HostPortCollisionDetected) {
    std::vector<Issue> issues;
    containers::load_manifest_string(
        "services:\n"
        "  a:\n    image: x:1\n    ports: ['9000:80']\n"
        "  b:\n    image: y:1\n    ports: ['9000:81']\n",
        &issues);
    EXPECT_TRUE(has_error(issues, "services.b.ports[0]"));
}

TEST(EngineValidation, SameContainerPortDifferentProtocolIsAllowed) {
    std::vector<Issue> issues;
    containers::load_manifest_string(
        "services:\n"
        "  dns:\n    image: coredns:1\n    ports: ['53:53/tcp', '53:53/udp']\n",
        &issues);
    EXPECT_TRUE(Manifest::is_ok(issues));
}

TEST(EngineValidation, UnpinnedImageWarns) {
    std::vector<Issue> issues;
    containers::load_manifest_string("services:\n  web:\n    image: nginx\n", &issues);
    bool warned = false;
    for (const auto& i : issues)
        if (i.severity == IssueSeverity::Warning && i.path == "services.web.image") warned = true;
    EXPECT_TRUE(warned);
    EXPECT_TRUE(Manifest::is_ok(issues));
}

TEST(EngineValidation, BadRestartPolicy) {
    std::vector<Issue> issues;
    containers::load_manifest_string(
        "services:\n  web:\n    image: nginx:1\n    restart: sometimes\n", &issues);
    EXPECT_TRUE(has_error(issues, "services.web.restart"));
}

TEST(PortParsing, TableOfForms) {
    Port p;
    ASSERT_TRUE(containers::parse_port("8080:80", &p));
    EXPECT_EQ(p.host, 8080);
    EXPECT_EQ(p.container, 80);
    EXPECT_EQ(p.protocol, "tcp");

    ASSERT_TRUE(containers::parse_port("127.0.0.1:5432:5432", &p));
    EXPECT_EQ(p.host, 5432);
    EXPECT_EQ(p.container, 5432);

    ASSERT_TRUE(containers::parse_port("53/udp", &p));
    EXPECT_EQ(p.host, 0);
    EXPECT_EQ(p.container, 53);
    EXPECT_EQ(p.protocol, "udp");

    EXPECT_FALSE(containers::parse_port("70000:80", &p));    // out of range
    EXPECT_FALSE(containers::parse_port("8080:0", &p));      // port 0
    EXPECT_FALSE(containers::parse_port("80/sctp", &p));     // protocol
    EXPECT_FALSE(containers::parse_port("a:b:c:d", &p));     // too many parts
    EXPECT_FALSE(containers::parse_port("", &p));
}

TEST(CommandSplit, RespectsQuotesAndEscapes) {
    const auto t = containers::split_command(R"(sh -c "echo hello world" --flag=x)");
    ASSERT_EQ(t.size(), 4u);
    EXPECT_EQ(t[0], "sh");
    EXPECT_EQ(t[1], "-c");
    EXPECT_EQ(t[2], "echo hello world");
    EXPECT_EQ(t[3], "--flag=x");

    EXPECT_TRUE(containers::split_command("   ").empty());
    const auto esc = containers::split_command(R"(a\ b c)");
    ASSERT_EQ(esc.size(), 2u);
    EXPECT_EQ(esc[0], "a b");
}

TEST(Inheritance, ChildOverridesParentAndMergesLists) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(
        "services:\n"
        "  base:\n"
        "    image: base:1\n"
        "    environment: {LOG_LEVEL: info, MODE: prod}\n"
        "    volumes: ['cache:/cache', 'data:/data']\n"
        "    restart: always\n"
        "  worker:\n"
        "    extends: base\n"
        "    image: worker:2\n"
        "    environment: {LOG_LEVEL: debug}\n"
        "    volumes: ['cache:/cache:ro']\n"
        "volumes:\n  cache: {}\n  data: {}\n",
        &issues);
    ASSERT_TRUE(Manifest::is_ok(issues));
    const auto inh = m.resolve_inheritance();
    EXPECT_TRUE(Manifest::is_ok(inh));

    const containers::Service* w = m.find_service("worker");
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->image, "worker:2");
    EXPECT_EQ(w->restart, RestartPolicy::Always);
    std::string level;
    ASSERT_TRUE(w->find_env("LOG_LEVEL", &level));
    EXPECT_EQ(level, "debug");
    ASSERT_EQ(w->mounts.size(), 2u);
    EXPECT_EQ(w->mounts[0].target, "/cache");
    EXPECT_TRUE(w->mounts[0].read_only);    // same target replaced in place
}

TEST(Inheritance, UnknownParentIsError) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(
        "services:\n  a:\n    extends: ghost\n    image: x:1\n", &issues);
    const auto inh = m.resolve_inheritance();
    EXPECT_TRUE(has_error(inh, "services.a.extends"));
}

TEST(Inheritance, CycleIsReported) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(
        "services:\n"
        "  a:\n    extends: b\n"
        "  b:\n    extends: a\n",
        &issues);
    const auto inh = m.resolve_inheritance();
    EXPECT_TRUE(has_error(inh, "extends"));
}

TEST(LaunchOrder, RespectsDependenciesAndIsStable) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(
        "services:\n"
        "  web:\n    image: web:1\n    depends_on: [api]\n"
        "  api:\n    image: api:1\n    depends_on: [db]\n"
        "  db:\n    image: db:1\n"
        "  metrics:\n    image: m:1\n",
        &issues);
    ASSERT_TRUE(Manifest::is_ok(issues));
    std::vector<Issue> order_issues;
    const auto order = m.launch_order(&order_issues);
    EXPECT_TRUE(Manifest::is_ok(order_issues));
    ASSERT_EQ(order.size(), 4u);
    const auto pos = [&](const std::string& n) {
        return std::distance(order.begin(), std::find(order.begin(), order.end(), n));
    };
    EXPECT_LT(pos("db"), pos("api"));
    EXPECT_LT(pos("api"), pos("web"));
    EXPECT_LT(pos("db"), pos("metrics"));    // unconstrained keeps declaration order
}

TEST(LaunchOrder, CycleYieldsEmptyOrderAndError) {
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_string(
        "services:\n"
        "  a:\n    image: x:1\n    depends_on: [b]\n"
        "  b:\n    image: y:1\n    depends_on: [a]\n",
        &issues);
    std::vector<Issue> order_issues;
    const auto order = m.launch_order(&order_issues);
    EXPECT_TRUE(order.empty());
    EXPECT_TRUE(has_error_anywhere(order_issues, "cycle"));
}

TEST(Repository, CheckedInConfigIsValid) {
    // The example manifest ships with the repo and must always stay valid.
    std::vector<Issue> issues;
    Manifest m = containers::load_manifest_file(
        std::string(CONTAINERS_SOURCE_DIR) + "/configs/stack.yaml", &issues);
    ASSERT_TRUE(Manifest::is_ok(issues));
    const auto inh = m.resolve_inheritance();
    EXPECT_TRUE(Manifest::is_ok(inh));
    std::vector<Issue> order_issues;
    const auto order = m.launch_order(&order_issues);
    EXPECT_TRUE(Manifest::is_ok(order_issues));
    EXPECT_EQ(order.size(), m.services().size());
}
