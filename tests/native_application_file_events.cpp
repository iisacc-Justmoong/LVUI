#include <LVUI/ApplicationFileEvents.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using Events = lvui::native::ApplicationFileEvents;
using ii::core::ErrorCode;
using ii::core::Result;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }

void ordered_and_reentrant() {
    std::vector<Events::Files> delivered;
    Events* current = nullptr;
    Events events([&](const auto& files) -> Result<void> {
        delivered.push_back(files);
        if (delivered.size() == 1) good(current->submit({"/later"}));
        return {};
    });
    current = &events;
    good(events.submit({"/한글.iisc", "/한글.iisc"}));
    good(events.submit({"/second"}));
    check(delivered.empty() && events.pending(), "File requests were delivered before admission completed");
    good(events.drain());
    check(delivered == std::vector<Events::Files>({{"/한글.iisc", "/한글.iisc"}, {"/second"}, {"/later"}}) && !events.pending(),
          "File requests lost batch order, duplicates, UTF-8, or reentrant delivery");
    good(events.submit({}));
    check(!events.pending(), "An empty file batch was delivered");
    good(events.close());
    check(!events.submit({"/late"}), "A retired event owner accepted files");
    auto late_abort = events.abort({ErrorCode::failed, "late"});
    check(!late_abort && late_abort.error().code == ErrorCode::closed && events.outcome(),
          "A retired event owner accepted a new terminal error");
}

void invalid_and_bounded() {
    auto reject = [](Events::Files input) {
        Events events([](const auto&) -> Result<void> { return {}; });
        auto failure = events.submit(std::move(input));
        check(!failure && failure.error().code == ErrorCode::invalid_argument && !events.outcome() && !events.submit({"/later"}),
              "An invalid file request did not close admission");
        good(events.close());
    };
    reject({""});
    reject({std::string("/x\0y", 4)});
    reject({std::string("\xff", 1)});
    reject({std::string(Events::maximum_path_bytes + 1, 'a')});
    reject(Events::Files(Events::maximum_files_per_batch + 1, "x"));
    Events batches([](const auto&) -> Result<void> { return {}; });
    for (std::size_t i = 0; i < Events::maximum_batches; ++i) good(batches.submit({"x"}));
    auto overflow = batches.submit({"y"});
    check(!overflow && overflow.error().code == ErrorCode::full && !batches.pending(), "The file batch limit lost its failure");
    good(batches.close());
    Events bytes([](const auto&) -> Result<void> { return {}; });
    for (std::size_t i = 0; i < Events::maximum_queued_bytes / Events::maximum_path_bytes; ++i)
        good(bytes.submit({std::string(Events::maximum_path_bytes, 'a')}));
    overflow = bytes.submit({"x"});
    check(!overflow && overflow.error().code == ErrorCode::full && !bytes.pending(), "The file byte limit was not enforced");
    good(bytes.close());
}

void errors_threads_and_reentry() {
    Events* pointer = nullptr;
    int count = 0;
    Events loop([&](const auto&) -> Result<void> { ++count; good(pointer->submit({"/again"})); return {}; });
    pointer = &loop;
    good(loop.submit({"/first"}));
    auto limited = loop.drain();
    check(!limited && limited.error().code == ErrorCode::full && count == static_cast<int>(Events::maximum_batches),
          "An endlessly reentrant callback was not bounded");
    good(loop.close());
    Events throws([](const auto&) -> Result<void> { throw std::runtime_error("file failure"); });
    good(throws.submit({"/file"}));
    auto failed = throws.drain();
    check(!failed && failed.error().cause && !throws.outcome(), "A file callback exception was lost");
    bool wrong = false;
    std::thread worker([&] { auto result = throws.submit({"/foreign"}); wrong = !result && result.error().code == ErrorCode::wrong_executor; });
    worker.join(); check(wrong, "A foreign thread controlled file delivery");
    good(throws.close());
}

void cancellation_never_destroys_the_active_callback() {
    Events* pointer = nullptr;
    int calls = 0;
    Events events([&](const auto&) -> Result<void> {
        ++calls;
        good(pointer->cancel());
        auto rejected = pointer->submit({"/late"});
        check(!rejected && rejected.error().code == ErrorCode::closed, "Cancellation admitted another file");
        check(!pointer->close(), "The active callback was destroyed inside its own delivery");
        return {};
    });
    pointer = &events;
    good(events.submit({"/current"})); good(events.submit({"/queued"}));
    good(events.drain());
    check(calls == 1 && !events.pending(), "Cancellation delivered a queued request");
    good(events.close());

    Events* failing_owner = nullptr;
    Events fails_after_cancel([&](const auto&) -> Result<void> {
        good(failing_owner->cancel());
        throw std::runtime_error("active callback failure");
    });
    failing_owner = &fails_after_cancel;
    good(fails_after_cancel.submit({"/file"}));
    auto failure = fails_after_cancel.drain();
    check(!failure && failure.error().cause && !fails_after_cancel.outcome(),
          "Cancellation hid an error from the active file callback");
    good(fails_after_cancel.close());
}
} // namespace
int main() { ordered_and_reentrant(); invalid_and_bounded(); errors_threads_and_reentry(); cancellation_never_destroys_the_active_callback(); }
