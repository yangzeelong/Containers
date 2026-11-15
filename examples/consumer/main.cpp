// Downstream consumer: proves that find_package(Containers CONFIG) exports a
// usable target (Containers::containers) and that the installed headers are
// self-contained.
#include "containers/Message.hpp"
#include "containers/SpscQueue.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int /*argc*/, char** /*argv*/) {
    // The message is copy-on-write: the copy shares one control block until
    // a mutable access detaches it.
    containers::Message original(containers::MakeMessage(std::string("hello containers"), "producer"));
    containers::Message copy = original;
    copy.Mut<std::string>() += " via find_package(Containers)";

    const containers::MessageMeta& meta = original.GetMetaData();
    std::printf("%-12s %s\n", "original", original.Borrow<std::string>().c_str());
    std::printf("  copy       : %s\n", copy.Borrow<std::string>().c_str());
    std::printf("  source     : %s\n", meta.sourceName.c_str());
    std::printf("  id         : %llu\n", static_cast<unsigned long long>(meta.messageId));

    // A queue of messages, then drain it the same way a worker does.
    containers::SpscQueue<containers::Message> queue(5);
    for (int i = 0; i < 5; ++i) {
        queue.push(containers::MakeMessage(i));
    }
    std::size_t drained = 0;
    bool drainedOne = false;
    for (;;) {
        containers::Message message;
        if (!queue.tryPop(message)) {
            break;
        }
        ++drained;
        drainedOne = true;
    }
    std::printf("  drained    : %zu\n", drained);
    return drainedOne && drained == 5 ? 0 : 1;
}
