#include "containers/stdx/StringView.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <unordered_set>

using containers::StringView;

TEST(StringViewTest, DefaultIsEmpty) {
    StringView view;
    EXPECT_TRUE(view.empty());
    EXPECT_EQ(0u, view.size());
    EXPECT_EQ(0u, view.length());
    EXPECT_EQ(nullptr, view.data());
    EXPECT_EQ(0u, static_cast<std::size_t>(view.end() - view.begin()));
    EXPECT_EQ("", view.toString());
}

TEST(StringViewTest, Constructors) {
    StringView fromCString("hello");
    EXPECT_EQ(5u, fromCString.size());
    EXPECT_EQ('h', fromCString[0]);
    EXPECT_EQ('o', fromCString[4]);

    const std::string text("hello world");
    StringView fromString(text);
    EXPECT_EQ(11u, fromString.size());
    EXPECT_EQ(text.data(), fromString.data());

    StringView fromLiteral("literal");
    EXPECT_EQ(7u, fromLiteral.size());

    char raw[] = {'a', 'b', 'c'};
    StringView fromArray(raw);
    EXPECT_EQ(3u, fromArray.size());
    EXPECT_EQ('c', fromArray[2]);

    StringView fromNull(nullptr);
    EXPECT_TRUE(fromNull.empty());
    EXPECT_EQ(nullptr, fromNull.data());
}

TEST(StringViewTest, EmbeddedNullBytesArePreserved) {
    const char raw[4] = {'a', '\0', 'b', 'c'};
    StringView view(raw, 4);
    EXPECT_EQ(4u, view.size());
    EXPECT_EQ('\0', view[1]);
    EXPECT_EQ('c', view[3]);
    EXPECT_NE(StringView("a"), view);
    EXPECT_EQ(1u, view.find('\0'));
    EXPECT_EQ(std::string(raw, 4), view.toString());
}

TEST(StringViewTest, SubstrClampsToSize) {
    const StringView view("abcdef");
    EXPECT_EQ("abc", view.substr(0, 3).toString());
    EXPECT_EQ("cdef", view.substr(2).toString());
    EXPECT_EQ("", view.substr(6).toString());
    EXPECT_EQ("", view.substr(100, 2).toString());
    EXPECT_EQ("abcdef", view.substr(0, StringView::npos).toString());
    EXPECT_EQ("", view.substr(0, 0).toString());

    StringView empty;
    EXPECT_TRUE(empty.substr(0).empty());
}

TEST(StringViewTest, RemovePrefixAndSuffix) {
    StringView view("abcdef");
    view.removePrefix(2);
    EXPECT_EQ("cdef", view.toString());
    view.removeSuffix(2);
    EXPECT_EQ("cd", view.toString());
    view.removePrefix(2);
    view.removeSuffix(0);
    EXPECT_TRUE(view.empty());
}

TEST(StringViewTest, CompareAndRelationalOperators) {
    const StringView abc("abc");
    const StringView abd("abd");
    const StringView shorter("ab");

    EXPECT_EQ(0, abc.compare(StringView("abc")));
    EXPECT_LT(abc.compare(abd), 0);
    EXPECT_GT(abd.compare(abc), 0);
    EXPECT_GT(abc.compare(shorter), 0);
    EXPECT_LT(shorter.compare(abc), 0);

    EXPECT_TRUE(abc == StringView("abc"));
    EXPECT_TRUE(abc != abd);
    EXPECT_TRUE(abc < abd);
    EXPECT_TRUE(abc <= abd);
    EXPECT_TRUE(abd > abc);
    EXPECT_TRUE(abd >= abc);

    EXPECT_TRUE(abc == "abc");
    EXPECT_TRUE("abc" == abc);
    EXPECT_TRUE(abc < "abd");
    EXPECT_FALSE(abc == "abd");
}

TEST(StringViewTest, StartsAndEndsWith) {
    const StringView view("containers");
    EXPECT_TRUE(view.startsWith(StringView("con")));
    EXPECT_TRUE(view.startsWith('c'));
    EXPECT_FALSE(view.startsWith('o'));
    EXPECT_TRUE(view.endsWith(StringView("ers")));
    EXPECT_TRUE(view.endsWith('s'));
    EXPECT_FALSE(view.endsWith('r'));
    EXPECT_TRUE(view.startsWith(StringView()));
    EXPECT_TRUE(view.endsWith(StringView()));
    EXPECT_FALSE(view.startsWith(StringView("containers!")));
    EXPECT_FALSE(view.endsWith(StringView("!containers")));
    EXPECT_TRUE(view.contains(StringView("tain")));
    EXPECT_FALSE(view.contains(StringView("tains!")));
}

TEST(StringViewTest, FindChar) {
    const StringView view("banana");
    EXPECT_EQ(0u, view.find('b'));
    EXPECT_EQ(1u, view.find('a'));
    EXPECT_EQ(1u, view.find('a', 0));
    EXPECT_EQ(3u, view.find('a', 2));
    EXPECT_EQ(5u, view.find('a', 4));
    EXPECT_EQ(StringView::npos, view.find('a', 6));
    EXPECT_EQ(StringView::npos, view.find('z'));
    EXPECT_EQ(StringView::npos, StringView().find('a'));
}

TEST(StringViewTest, FindSubstring) {
    const StringView view("banana");
    EXPECT_EQ(2u, view.find(StringView("nana")));
    EXPECT_EQ(StringView::npos, view.find(StringView("nana"), 3));
    EXPECT_EQ(StringView::npos, view.find(StringView("apple")));
    EXPECT_EQ(0u, view.find(StringView()));
    EXPECT_EQ(5u, view.find(StringView(), 5));
    EXPECT_EQ(StringView::npos, view.find(StringView(), 7));
    EXPECT_EQ(StringView::npos, StringView().find(StringView("a")));
}

TEST(StringViewTest, Rfind) {
    const StringView view("banana");
    EXPECT_EQ(5u, view.rfind('a'));
    EXPECT_EQ(3u, view.rfind('a', 4));
    EXPECT_EQ(1u, view.rfind('a', 1));
    EXPECT_EQ(4u, view.rfind(StringView("na")));
    EXPECT_EQ(2u, view.rfind(StringView("na"), 3));
    EXPECT_EQ(StringView::npos, view.rfind('z'));
    EXPECT_EQ(StringView::npos, view.rfind(StringView("apple")));
    EXPECT_EQ(6u, view.rfind(StringView()));
    EXPECT_EQ(2u, view.rfind(StringView(), 2));
    EXPECT_EQ(6u, view.rfind(StringView(), 954));
    EXPECT_EQ(StringView::npos, StringView().rfind('a'));
}

TEST(StringViewTest, HashAndUnorderedSet) {
    const StringView alpha("alpha");
    const StringView alphaCopy(alpha);
    EXPECT_EQ(containers::StringViewHash()(alpha), containers::StringViewHash()(alphaCopy));
    EXPECT_EQ(std::hash<StringView>()(alpha), containers::StringViewHash()(alpha));

    std::unordered_set<StringView, containers::StringViewHash> set;
    set.insert(alpha);
    set.insert(StringView("beta"));
    EXPECT_EQ(2u, set.size());
    EXPECT_EQ(1u, set.count(StringView("alpha")));
    EXPECT_EQ(0u, set.count(StringView("gamma")));
}

TEST(StringViewTest, StreamInsertion) {
    std::ostringstream stream;
    stream << StringView("hello") << ' ' << StringView("world");
    EXPECT_EQ("hello world", stream.str());
}

TEST(StringViewTest, UserDefinedLiteral) {
    using namespace containers::string_view_literals;
    const StringView view = "literal"_sv;
    EXPECT_EQ(7u, view.size());
    EXPECT_EQ("literal", view.toString());
}

TEST(StringViewTest, ConstexprConstruction) {
    constexpr StringView empty;
    static_assert(empty.size() == 0, "default view must be empty");
    constexpr StringView prefix("abcdef", 3);
    static_assert(prefix.size() == 3, "explicit size must be preserved");
}
