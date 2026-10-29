#include <LVUI/NativeBindings.h>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <thread>

namespace lvui::native {
void ReadContext::track(detail::SourceRead read) {
    retain(read.owner);retain(read.value_owner);
    if (std::ranges::none_of(reads_, [&](const auto& previous) { return previous.identity == read.identity; })){
        if(reads_.size()==Bindings::maximum_sources)throw std::length_error("Native read source capacity exhausted");
        reads_.push_back(std::move(read));
    }
}
namespace detail {
struct BindingState final {
    using Update = std::function<ii::core::Result<void>()>;
    using Prepare = std::function<ii::core::Result<Update>(ReadContext&)>;
    struct Node final {
        Prepare prepare;
        std::function<void()> close;
        std::set<const void*> sources;
        std::optional<ii::core::Error> error;
        bool derived = false;
    };
    struct Source final {
        ii::core::ObservableSubscription subscription;
        std::set<BindingId> readers;
    };
    std::thread::id owner = std::this_thread::get_id();
    std::atomic_bool active{true};
    std::weak_ptr<BindingState> self;
    std::map<BindingId, std::shared_ptr<Node>> nodes;
    std::map<const void*, Source> sources;
    std::set<BindingId> derived, effects;
    BindingId next = 0;
    std::size_t batch_depth = 0;
    bool draining = false;
    std::uint64_t evaluations = 0, applications = 0;

    ii::core::Result<void> check() const {
        if (owner != std::this_thread::get_id())
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Native bindings require their owner thread");
        if (!active) return ii::core::fail(ii::core::ErrorCode::closed, "Native bindings are closed");
        return {};
    }
    static ii::core::Error exception(std::string message) {
        auto cause = std::current_exception();
        try { if (cause) std::rethrow_exception(cause); }
        catch (const std::exception& error) { message += ": "; message += error.what(); }
        catch (...) {}
        return {ii::core::ErrorCode::failed, std::move(message), std::move(cause)};
    }
    void enqueue(BindingId id) {
        const auto found = nodes.find(id);
        if (found != nodes.end()) (found->second->derived ? derived : effects).insert(id);
    }
    void invalidate(const void* source) {
        if (!active) return;
        const auto found = sources.find(source);
        if (found == sources.end()) return;
        for (auto id : found->second.readers) enqueue(id);
        if (!batch_depth && !draining) (void)drain();
    }
    void release_sources(BindingId id, Node& node, const std::set<const void*>& retained = {}) {
        for (const auto key : node.sources) {
            if (retained.contains(key)) continue;
            const auto source = sources.find(key);
            if (source == sources.end()) continue;
            source->second.readers.erase(id);
            if (source->second.readers.empty()) sources.erase(source);
        }
    }
    ii::core::Result<bool> dependencies(BindingId id, Node& node, const ReadContext& context) {
        std::set<const void*> required;
        std::map<const void*, Source> created;
        std::vector<std::pair<Source*, std::set<BindingId>>> readers;
        try {
            for (const auto& read : context.reads_) {
                required.insert(read.identity);
                auto source = sources.find(read.identity);
                if (source == sources.end() || !source->second.subscription.active()) {
                    if (source == sources.end() && sources.size() + created.size() >= Bindings::maximum_sources)
                        throw std::length_error("Native binding source capacity exhausted");
                    const auto weak = self;
                    auto subscription = read.subscribe([weak, key = read.identity] {
                        if (auto state = weak.lock()) state->invalidate(key);
                    });
                    if (!subscription.active()) throw std::logic_error("Native binding source is closed");
                    auto next_readers = source == sources.end() ? std::set<BindingId>{} : source->second.readers;
                    next_readers.insert(id);
                    created.emplace(read.identity, Source{std::move(subscription), std::move(next_readers)});
                } else {
                    std::set<BindingId> next_readers;
                    if (!source->second.readers.contains(id)) next_readers.insert(id);
                    readers.emplace_back(&source->second, std::move(next_readers));
                }
            }
            bool stable = true;
            for (const auto& read : context.reads_) stable &= read.current_revision() == read.revision;
            // All allocations, foreign calls and revision checks precede this
            // no-throw commit. A failed subscription preserves the old graph.
            for (auto& [source, next_readers] : readers) source->readers.merge(next_readers);
            for (auto& [key, source] : created)
                if (auto existing = sources.find(key); existing != sources.end())
                    existing->second = std::move(source);
            sources.merge(created);
            release_sources(id, node, required);
            node.sources = std::move(required);
            return stable;
        } catch (...) {
            return std::unexpected(exception("Native binding dependency subscription failed"));
        }
    }
    ii::core::Result<void> evaluate(BindingId id, const std::shared_ptr<Node>& node) {
        return ii::core::ObservableReadScope::pause([&]() -> ii::core::Result<void>{
        ReadContext context;
        ii::core::Result<Update> update = ii::core::fail(ii::core::ErrorCode::failed, "Native binding did not evaluate");
        try { update = node->prepare(context); }
        catch (...) { update = std::unexpected(exception("Native binding getter failed")); }
        ++evaluations;
        if (!active || !nodes.contains(id)) return {};
        const auto subscribed = dependencies(id, *node, context);
        if (!subscribed) { node->error = subscribed.error(); return std::unexpected(subscribed.error()); }
        if (!*subscribed) { enqueue(id); return {}; }
        if (!update) { node->error = update.error(); return std::unexpected(update.error()); }
        ii::core::Result<void> applied;
        try { if (*update) { applied = (*update)(); ++applications; } }
        catch (...) { applied = std::unexpected(exception("Native binding setter failed")); }
        if (!active || !nodes.contains(id)) return {};
        if (!applied) { node->error = applied.error(); return applied; }
        node->error.reset();
        return {};
        });
    }
    ii::core::Result<void> drain() {
        if (auto valid = check(); !valid) return valid;
        if (draining || batch_depth) return {};
        draining = true;
        struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{draining};
        std::optional<ii::core::Error> failure;
        std::size_t count = 0;
        while (active && (!derived.empty() || !effects.empty())) {
            if (++count > Bindings::maximum_evaluations_per_flush) {
                const ii::core::Error cycle{ii::core::ErrorCode::cycle, "Native binding update cycle exceeded its bound"};
                for (auto id : derived) if (nodes.contains(id)) nodes.at(id)->error = cycle;
                for (auto id : effects) if (nodes.contains(id)) nodes.at(id)->error = cycle;
                derived.clear(); effects.clear();
                return std::unexpected(cycle);
            }
            auto& pending = derived.empty() ? effects : derived;
            const auto id = *pending.begin(); pending.erase(pending.begin());
            const auto found = nodes.find(id);
            if (found == nodes.end()) continue;
            const auto node = found->second;
            auto result = evaluate(id, node);
            if (!result && !failure) failure = result.error();
        }
        if (failure) return std::unexpected(std::move(*failure));
        return {};
    }
    ii::core::Result<void> erase(BindingId id) {
        if (auto valid = check(); !valid) return valid;
        const auto found = nodes.find(id);
        if (found == nodes.end()) return {};
        const auto node = found->second;
        nodes.erase(found);
        derived.erase(id); effects.erase(id);
        release_sources(id, *node);
        if (node->close) node->close();
        return {};
    }
    void stop() {
        active = false;
        sources.clear(); derived.clear(); effects.clear();
        auto retiring = std::move(nodes);
        for (const auto& [id, node] : retiring) if (node->close) node->close();
    }
};
} // namespace detail
Bindings::Bindings() : state_(std::make_shared<detail::BindingState>()) { state_->self = state_; }
std::shared_ptr<const std::atomic_bool> Bindings::lifetime() const { return {state_, &state_->active}; }
Bindings::~Bindings() {
    if (state_->owner == std::this_thread::get_id()) state_->stop();
    else state_->active = false;
}
ii::core::Result<BindingId> Bindings::add(Prepare prepare, bool derived, std::function<void()> close) {
    auto state = state_;
    if (auto valid = state->check(); !valid) return std::unexpected(valid.error());
    if (state->nodes.size() == maximum_bindings || state->next == UINT64_MAX)
        return ii::core::fail(ii::core::ErrorCode::busy, "Native binding capacity exhausted");
    const auto id = ++state->next;
    auto node = std::make_shared<detail::BindingState::Node>();
    node->prepare = std::move(prepare); node->derived = derived; node->close = std::move(close);
    state->nodes.emplace(id, node);
    const bool already_draining = state->draining;
    state->draining = true;
    auto initialized = state->evaluate(id, node);
    std::size_t attempts = 0;
    while (initialized && state->active && state->nodes.contains(id) &&
           (state->derived.erase(id) || state->effects.erase(id))) {
        if (++attempts == maximum_evaluations_per_flush) {
            initialized = ii::core::fail(ii::core::ErrorCode::cycle, "Native binding initializer did not settle");
            break;
        }
        initialized = state->evaluate(id, node);
    }
    state->draining = already_draining;
    if (!initialized) { (void)state->erase(id); return std::unexpected(initialized.error()); }
    if (!state->active || !state->nodes.contains(id))
        return ii::core::fail(ii::core::ErrorCode::closed, "Native binding closed during initialization");
    if (!already_draining && !state->batch_depth) {
        auto flushed = state->drain();
        if (!flushed) { (void)state->erase(id); return std::unexpected(flushed.error()); }
    }
    return id;
}
ii::core::Result<void> Bindings::remove(BindingId id) { return state_->erase(id); }
ii::core::Result<void> Bindings::begin_batch() {
    if (auto valid = state_->check(); !valid) return valid;
    if (state_->batch_depth == std::numeric_limits<std::size_t>::max())
        return ii::core::fail(ii::core::ErrorCode::busy, "Native binding batch depth exhausted");
    ++state_->batch_depth;
    return {};
}
ii::core::Result<void> Bindings::end_batch() {
    if (auto valid = state_->check(); !valid) return valid;
    if (!state_->batch_depth) return ii::core::fail(ii::core::ErrorCode::failed, "Native binding batch is not open");
    --state_->batch_depth;
    return state_->drain();
}
ii::core::Result<void> Bindings::flush() { return state_->drain(); }
bool Bindings::available() const noexcept {
    return state_->owner == std::this_thread::get_id() && state_->active.load();
}
ii::core::Result<void> Bindings::close() {
    if (state_->owner != std::this_thread::get_id())
        return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Native bindings require their owner thread");
    if (state_->active) state_->stop();
    return {};
}
ii::core::Result<BindingStats> Bindings::stats() const {
    if (auto valid = state_->check(); !valid) return std::unexpected(valid.error());
    return BindingStats{state_->nodes.size(), state_->sources.size(), state_->derived.size() + state_->effects.size(),
                        state_->evaluations, state_->applications};
}
ii::core::Result<std::vector<BindingFailure>> Bindings::failures() const {
    if (auto valid = state_->check(); !valid) return std::unexpected(valid.error());
    std::vector<BindingFailure> result;
    for (const auto& [id, node] : state_->nodes) if (node->error) result.push_back({id, *node->error});
    return result;
}
} // namespace lvui::native
