#include <LVUI/NativeBindings.h>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <thread>
#include <vector>

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
    bool effects_retired = false;
    std::uint64_t evaluations = 0, applications = 0;
    std::uint64_t invalidations = 0, coalesced = 0, flushes = 0, failed_flushes = 0;
    std::size_t high_watermark = 0, cycle_steps = 0;
    std::vector<BindingId> evaluating;

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
        if (found != nodes.end()) {
            if (effects_retired && !found->second->derived) return;
            const bool inserted = (found->second->derived ? derived : effects).insert(id).second;
            if (!inserted) ++coalesced;
            high_watermark = std::max(high_watermark, derived.size() + effects.size());
        }
    }
    void invalidate(const void* source) {
        if (!active) return;
        const auto found = sources.find(source);
        if (found == sources.end()) return;
        ++invalidations;
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
        if (evaluating.size() == 64 || std::ranges::find(evaluating, id) != evaluating.end()) {
            node->error = ii::core::Error{ii::core::ErrorCode::cycle, "Native binding evaluation reentered itself"};
            return std::unexpected(*node->error);
        }
        evaluating.push_back(id);
        struct Evaluation final { std::vector<BindingId>& stack; ~Evaluation() { stack.pop_back(); } } evaluation{evaluating};
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
    ii::core::Result<void> drain(bool derived_only = false, bool ignore_repeated_failures = false) {
        if (auto valid = check(); !valid) return valid;
        if ((!derived_only && (draining || batch_depth)) || (derived_only && derived.empty())) return {};
        const bool outer = !draining;
        if (outer) { draining = true; cycle_steps = 0; }
        if (!derived.empty() || (!derived_only && !effects.empty())) ++flushes;
        struct Reset final { bool& flag; bool outer; ~Reset() { if (outer) flag = false; } } reset{draining,outer};
        std::optional<ii::core::Error> failure;
        bool had_failure = false;
        while (active && (!derived.empty() || (!derived_only && !effects.empty()))) {
            if (++cycle_steps > Bindings::maximum_evaluations_per_flush) {
                const ii::core::Error cycle{ii::core::ErrorCode::cycle, "Native binding update cycle exceeded its bound"};
                for (auto id : derived) if (nodes.contains(id)) nodes.at(id)->error = cycle;
                for (auto id : effects) if (nodes.contains(id)) nodes.at(id)->error = cycle;
                derived.clear(); effects.clear();
                ++failed_flushes;
                return std::unexpected(cycle);
            }
            auto& pending = derived.empty() ? effects : derived;
            const auto id = *pending.begin(); pending.erase(pending.begin());
            const auto found = nodes.find(id);
            if (found == nodes.end()) continue;
            const auto node = found->second;
            const auto previous = ignore_repeated_failures ? node->error : std::optional<ii::core::Error>{};
            ii::core::Result<void> result;
            try { result = evaluate(id, node); }
            catch (...) {
                node->error = exception("Native binding evaluation failed");
                result = std::unexpected(*node->error);
            }
            if (!result) {
                had_failure = true;
                if (!failure) {
                    const auto& error = result.error();
                    const bool repeated = ignore_repeated_failures && previous && error.code != ii::core::ErrorCode::cycle &&
                        previous->code == error.code && previous->message == error.message;
                    if (!repeated) failure = error;
                }
            }
        }
        if (had_failure) ++failed_flushes;
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
    if (!derived && state->effects_retired)
        return ii::core::fail(ii::core::ErrorCode::closed, "Native presentation effects are retired");
    if (state->nodes.size() == maximum_bindings || state->next == UINT64_MAX)
        return ii::core::fail(ii::core::ErrorCode::busy, "Native binding capacity exhausted");
    const auto id = ++state->next;
    auto node = std::make_shared<detail::BindingState::Node>();
    node->prepare = std::move(prepare); node->derived = derived; node->close = std::move(close);
    state->nodes.emplace(id, node);
    const bool already_draining = state->draining;
    if (!already_draining) state->cycle_steps = 0;
    state->draining = true;
    ii::core::Result<void> initialized;
    try {
        initialized = state->evaluate(id, node);
        std::size_t attempts = 0;
        while (initialized && state->active && state->nodes.contains(id) &&
               (state->derived.erase(id) || state->effects.erase(id))) {
            if (++attempts == maximum_evaluations_per_flush) {
                initialized = ii::core::fail(ii::core::ErrorCode::cycle, "Native binding initializer did not settle");
                break;
            }
            initialized = state->evaluate(id, node);
        }
    } catch (...) { initialized = std::unexpected(detail::BindingState::exception("Native binding initialization failed")); }
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
ii::core::Result<void> Bindings::remove(BindingId id) { auto state = state_; return state->erase(id); }
ii::core::Result<void> Bindings::begin_batch() {
    if (auto valid = state_->check(); !valid) return valid;
    if (state_->batch_depth == std::numeric_limits<std::size_t>::max())
        return ii::core::fail(ii::core::ErrorCode::busy, "Native binding batch depth exhausted");
    ++state_->batch_depth;
    return {};
}
ii::core::Result<void> Bindings::end_batch() {
    auto state = state_;
    if (auto valid = state->check(); !valid) return valid;
    if (!state->batch_depth) return ii::core::fail(ii::core::ErrorCode::failed, "Native binding batch is not open");
    --state->batch_depth;
    return state->drain();
}
ii::core::Result<void> Bindings::end_callback_batch() {
    auto state = state_;
    if (auto valid = state->check(); !valid) return valid;
    if (!state->batch_depth) return ii::core::fail(ii::core::ErrorCode::failed, "Native binding batch is not open");
    --state->batch_depth;
    return state->drain(false, true);
}
ii::core::Result<void> Bindings::flush() { auto state = state_; return state->drain(); }
ii::core::Result<void> Bindings::settle_derived() { auto state = state_; return state->drain(true); }
ii::core::Result<void> Bindings::retire_effects() {
    auto state = state_;
    if (auto valid = state->check(); !valid) return valid;
    if (state->effects_retired) return {};
    std::vector<BindingId> retiring;
    try {
        retiring.reserve(state->nodes.size());
        for (const auto& [id, node] : state->nodes) if (!node->derived) retiring.push_back(id);
    } catch (...) {
        return ii::core::fail(ii::core::ErrorCode::failed, "Cannot retire native presentation effects", std::current_exception());
    }
    state->effects_retired = true;
    state->effects.clear();
    for (auto id : retiring) {
        if (!state->active) return {};
        if (auto removed = state->erase(id); !removed) return removed;
    }
    return {};
}
bool Bindings::available() const noexcept {
    return state_->owner == std::this_thread::get_id() && state_->active.load();
}
ii::core::Result<void> Bindings::close() {
    auto state = state_;
    if (state->owner != std::this_thread::get_id())
        return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Native bindings require their owner thread");
    if (state->active) state->stop();
    return {};
}
ii::core::Result<BindingStats> Bindings::stats() const {
    if (auto valid = state_->check(); !valid) return std::unexpected(valid.error());
    return BindingStats{state_->nodes.size(), state_->sources.size(), state_->derived.size() + state_->effects.size(),
                        state_->evaluations, state_->applications, state_->invalidations, state_->coalesced,
                        state_->flushes, state_->failed_flushes, state_->high_watermark, state_->batch_depth,
                        state_->draining};
}
ii::core::Result<std::vector<BindingFailure>> Bindings::failures() const {
    if (auto valid = state_->check(); !valid) return std::unexpected(valid.error());
    std::vector<BindingFailure> result;
    for (const auto& [id, node] : state_->nodes) if (node->error) result.push_back({id, *node->error});
    return result;
}

namespace detail {
struct BindingDiagnosticsState final {
    struct Entry final {Bindings* identity;std::weak_ptr<Bindings> graph;};
    const std::thread::id owner=std::this_thread::get_id();
    bool active=true;
    BindingActivationId next=0;
    std::map<BindingActivationId,Entry> entries;
    std::map<Bindings*,BindingActivationId,std::less<>> identities;
    [[nodiscard]] ii::core::Result<void> check()const{
        if(owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Binding diagnostics require their owner thread");
        if(!active)return ii::core::fail(ii::core::ErrorCode::closed,"Binding diagnostics are closed");return {};
    }
    void prune(){
        for(auto item=entries.begin();item!=entries.end();){
            auto graph=item->second.graph.lock();
            if(!graph||!graph->available()){identities.erase(item->second.identity);item=entries.erase(item);}
            else ++item;
        }
    }
};
}
BindingDiagnostics::BindingDiagnostics():state_(std::make_shared<detail::BindingDiagnosticsState>()){}
BindingDiagnostics::~BindingDiagnostics()=default;
ii::core::Result<BindingActivationId> BindingDiagnostics::attach(const std::shared_ptr<Bindings>& graph){
    auto state=state_;if(auto valid=state->check();!valid)return std::unexpected(valid.error());
    if(!graph)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Binding diagnostics require an activation graph");
    if(auto status=graph->stats();!status)return std::unexpected(status.error());
    try{
        if(auto previous=state->identities.find(graph.get());previous!=state->identities.end()){
            const auto entry=state->entries.find(previous->second);auto retained=entry==state->entries.end()?std::shared_ptr<Bindings>{}:entry->second.graph.lock();
            if(retained&&retained->available())return ii::core::fail(ii::core::ErrorCode::already_exists,"Binding activation is already registered");
            if(entry!=state->entries.end())state->entries.erase(entry);state->identities.erase(previous);
        }
        if(state->entries.size()>=maximum_activations)state->prune();
        if(state->entries.size()>=maximum_activations||state->next==std::numeric_limits<BindingActivationId>::max())
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Binding diagnostics activation limit exceeded");
        const auto id=state->next+1;auto [entry,inserted]=state->entries.emplace(id,detail::BindingDiagnosticsState::Entry{graph.get(),graph});
        if(!inserted)return ii::core::fail(ii::core::ErrorCode::already_exists,"Binding activation identity is already registered");
        try{state->identities.emplace(graph.get(),id);}catch(...){state->entries.erase(entry);throw;}
        state->next=id;return id;
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Binding diagnostics registration failed",std::current_exception());}
}
ii::core::Result<void> BindingDiagnostics::detach(BindingActivationId id){
    auto state=state_;if(auto valid=state->check();!valid)return valid;
    const auto found=state->entries.find(id);if(found==state->entries.end())return ii::core::fail(ii::core::ErrorCode::not_found,"Binding activation is not registered");
    state->identities.erase(found->second.identity);state->entries.erase(found);return {};
}
ii::core::Result<std::size_t> BindingDiagnostics::activation_count(){
    auto state=state_;if(auto valid=state->check();!valid)return std::unexpected(valid.error());
    state->prune();return state->entries.size();
}
ii::core::Result<std::vector<ScopedBindingFailure>> BindingDiagnostics::failures(){
    auto state=state_;if(auto valid=state->check();!valid)return std::unexpected(valid.error());
    try{
        state->prune();std::vector<ScopedBindingFailure> result;
        for(const auto& [id,entry]:state->entries){
            auto graph=entry.graph.lock();if(!graph)continue;auto failures=graph->failures();if(!failures)return std::unexpected(failures.error());
            if(failures->size()>maximum_failures-result.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Binding diagnostics failure limit exceeded");
            for(auto& failure:*failures)result.push_back({id,std::move(failure)});
        }
        return result;
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Binding diagnostics query failed",std::current_exception());}
}
ii::core::Result<void> BindingDiagnostics::close(){
    auto state=state_;if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Binding diagnostics require their owner thread");
    state->active=false;state->entries.clear();state->identities.clear();return {};
}
} // namespace lvui::native
