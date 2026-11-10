#include <LVUI/ApplicationCommands.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using Commands = lvui::native::ApplicationCommands;
using ii::core::Error;
using ii::core::ErrorCode;
using ii::core::Result;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }

void deferred_coalescing_and_retry() {
    std::vector<Commands::Task> tasks;
    std::vector<Error> errors;
    int calls = 0;
    std::shared_ptr<Commands> commands;
    commands = std::make_shared<Commands>([&](auto task) -> Result<void> { tasks.push_back(std::move(task)); return {}; },
        [&]() -> Result<void> { ++calls; good(commands->quit()); return {}; },
        [&](const auto& error) { errors.push_back(error); });
    good(commands->quit()); good(commands->quit());
    check(tasks.size() == 1 && calls == 0, "Quit ran inline or did not coalesce");
    auto first = std::move(tasks.back()); tasks.pop_back(); first();
    check(calls == 1 && tasks.empty() && errors.empty(), "A nested quit was not coalesced");
    good(commands->quit());
    auto retry = std::move(tasks.back()); tasks.pop_back(); retry();
    check(calls == 2 && errors.empty(), "A later quit could not retry a vetoed close");
    good(commands->quit()); good(commands->cancel());
    auto canceled = std::move(tasks.back()); tasks.pop_back(); canceled();
    check(calls == 2 && !commands->quit(), "Cancellation delivered or accepted a later quit");
}

void failures_and_lifetime() {
    std::vector<Commands::Task> tasks;
    std::vector<Error> errors;
    bool reject = true;
    auto commands = std::make_shared<Commands>([&](auto task) -> Result<void> {
        tasks.push_back(std::move(task));
        return reject ? ii::core::fail(ErrorCode::full, "queue full") : Result<void>{};
    }, []() -> Result<void> { throw std::runtime_error("close failed"); },
       [&](const auto& error) { errors.push_back(error); });
    auto rejected = commands->quit();
    check(!rejected && rejected.error().code == ErrorCode::full, "Dispatcher failure was hidden");
    tasks.back()(); tasks.clear();
    check(errors.empty(), "A rejected queued task ran later");
    reject = false; good(commands->quit());
    auto task = std::move(tasks.back()); tasks.clear(); task();
    check(errors.size() == 1 && errors[0].cause && !commands->outcome() && !commands->quit(),
          "A close exception was not preserved or admission remained open");
    bool wrong_thread = false;
    std::thread foreign([&] { auto result = commands->cancel(); wrong_thread = !result && result.error().code == ErrorCode::wrong_executor; });
    foreign.join(); check(wrong_thread, "A foreign thread controlled application commands");
    good(commands->cancel());

    auto inline_dispatch = std::make_shared<Commands>([](auto work) -> Result<void> { work(); return {}; },
        []() -> Result<void> { throw std::runtime_error("must not run inline"); }, [](const auto&) {});
    check(!inline_dispatch->quit(), "Inline dispatch was accepted");
    good(inline_dispatch->cancel());

    std::vector<Commands::Task> late;
    int invoked = 0;
    auto retired = std::make_shared<Commands>([&](auto work) -> Result<void> { late.push_back(std::move(work)); return {}; },
        [&]() -> Result<void> { ++invoked; return {}; }, [](const auto&) {});
    good(retired->quit()); retired.reset(); late.front()();
    check(invoked == 0, "A queued command retained a retired owner");
}

void cancellation_preserves_active_failure() {
    std::vector<Commands::Task> tasks;
    std::vector<Error> errors;
    std::shared_ptr<Commands> commands;
    commands = std::make_shared<Commands>([&](auto task) -> Result<void> { tasks.push_back(std::move(task)); return {}; },
        [&]() -> Result<void> { good(commands->cancel()); throw std::runtime_error("failure after cancel"); },
        [&](const auto& error) { errors.push_back(error); });
    good(commands->quit()); tasks.front()();
    check(errors.size() == 1 && errors[0].cause && !commands->outcome(),
          "Cancellation erased an active command failure");
}
}
int main() { deferred_coalescing_and_retry(); failures_and_lifetime(); cancellation_preserves_active_failure(); }
