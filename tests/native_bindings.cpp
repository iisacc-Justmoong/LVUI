#include <LVUI/NativeBindings.h>
#include <LVUI/NativeValues.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace lvui::native;
using ii::core::Observable;
using ii::core::Result;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T require(Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    if constexpr (!std::is_void_v<T>) return std::move(*result);
}
void selective() {
    Bindings bindings;
    Observable<int> left{1}, right{2};
    int a = 0, b = 0, calls = 0;
    require(bindings.bind([&](ReadContext& c) { ++calls; return c.read(left) * 2; },
                          [&](int n) -> Result<void> { a = n; return {}; }));
    require(bindings.bind([&](ReadContext& c) { return c.read(right); },
                          [&](int n) -> Result<void> { b = n; return {}; }));
    check(a == 2 && b == 2 && calls == 1, "Initial binding was not applied");
    left.set(4);
    check(a == 8 && b == 2 && calls == 2, "A source change rebuilt unrelated values");
    left.set(4);
    check(calls == 2, "Unchanged source was polled");
    auto stats = require(bindings.stats());
    check(stats.evaluations == 3 && stats.applications == 3, "Binding counts do not represent actual work");
    std::string backing = "first";
    Observable<int> revision{0};
    std::string owned;
    require(bindings.bind([&](ReadContext& c) { (void)c.read(revision); return std::string_view(backing); },
                          [&](const std::string& s) -> Result<void> { owned = s; return {}; }));
    backing = "next"; revision.set(1);
    check(owned == "next", "Borrowed String was not snapshotted");
}
void readonly() {
    Bindings graph;
    Observable<int> source{3};const auto& value=source;
    int shown=0,calls=0;
    require(graph.bind([&](ReadContext& context){++calls;const auto observed=read_source(context,value);check(retained_initial(context,value)==observed,"A readonly retained read lost its const observer contract");return observed;},
        [&](int next)->Result<void>{shown=next;return {};}));
    source.set(7);
    check(shown==7&&calls==2&&require(graph.stats()).sources==1,
        "A readonly native member silently became a nonreactive snapshot");
}
void readonly_members() {
    struct Item final {Observable<int> count{3};};
    Bindings graph;auto mutable_item=std::make_shared<Item>();
    Observable<std::shared_ptr<const Item>> source{mutable_item};
    auto selected=require(graph.derive([&](ReadContext& context){return value_copy(context.read(source));}));
    int shown=0,calls=0;
    require(graph.bind([&](ReadContext& context){++calls;return read_source(context,member(context.read(*selected),[](const auto& item)->decltype(auto){return (item.count);}));},
        [&](int next)->Result<void>{shown=next;return {};}));
    mutable_item->count.set(7);
    check(shown==7&&calls==2,"A readonly selected object's nested Observable did not publish");
    auto replacement=std::make_shared<Item>();replacement->count.set(11);source.set(replacement);
    mutable_item->count.set(9);
    check(shown==11&&calls==3,"Replacing a selected object retained its old native member dependency");
    replacement->count.set(13);check(shown==13&&calls==4,"A replacement readonly member was not observed");
}
void readonly_reentry() {
    struct Item final {explicit Item(int value):count(value){}Observable<int> count;};
    Bindings graph;
    Observable<std::shared_ptr<const Item>> selected{std::make_shared<Item>(3)};
    std::weak_ptr<const Item> previous=selected.get();
    bool switched=false;int shown=0;
    require(graph.bind([&](ReadContext& context){
        auto value=read_source(context,member(context.read(selected),[](const auto& item)->decltype(auto){return (item.count);}));
        if(!switched){switched=true;selected.set(std::make_shared<Item>(9));}
        return value;
    },[&](int value)->Result<void>{shown=value;return {};}));
    check(shown==9&&previous.expired(),"A reentrant native selection did not settle and retire its prior owner");
    require(graph.close());
}
void readonly_owners() {
    struct Item final {int count=7;};
    Bindings graph;Observable<int> mode{0};std::weak_ptr<Item> temporary;int shown=0;
    require(graph.bind([&](ReadContext& context){
        const auto kind=context.read(mode);
        auto owner=std::make_shared<Item>();temporary=owner;
        for(std::size_t i=0;i<ReadContext::maximum_retained_values+1;++i)context.retain(owner);
        if(kind)for(std::size_t i=0;i<ReadContext::maximum_retained_values;++i)context.retain(std::make_shared<Item>());
        return read_source(context,member(context,owner,[](const auto& item)->decltype(auto){return (item.count);}));
    },[&](int value)->Result<void>{check(!temporary.expired(),"Native read owner retired before application completed");shown=value;return {};}));
    check(shown==7&&temporary.expired(),"Native read owners escaped their evaluation lifetime");
    mode.set(1);check(shown==7&&temporary.expired()&&require(graph.failures()).size()==1,"Read ownership bounds leaked owners or committed a failed evaluation");
    mode.set(0);check(temporary.expired()&&require(graph.failures()).empty(),"Read ownership did not recover after capacity rejection");
    int retired=0;
    std::shared_ptr<void()> function(+[]{},[&](auto){++retired;});std::weak_ptr<void()> weak_function=function;
    Bindings functions;
    require(functions.bind([&](ReadContext& context){context.retain(function);function.reset();return 1;},
        [&](int)->Result<void>{check(!weak_function.expired(),"A function-pointer control block retired during application");return {};}));
    check(weak_function.expired()&&retired==1,"Native function ownership escaped its evaluation");
}
void readonly_references() {
    struct Item final {Observable<int> count{3};};Item item;const auto reference=std::cref(item);
    Bindings graph;int shown=0;
    require(graph.bind([&](ReadContext& context){return read_source(context,member(context,reference,[](const auto& item)->decltype(auto){return (item.count);}));},
        [&](int value)->Result<void>{shown=value;return {};}));
    item.count.set(9);check(shown==9,"A standard readonly reference wrapper lost its native member publication");
}
void branches() {
    Bindings bindings;
    Observable<bool> choose{true};
    Observable<int> a{2}, b{5};
    int value = 0, calls = 0;
    require(bindings.bind([&](ReadContext& c) { ++calls; return c.read(choose) ? c.read(a) : c.read(b); },
                          [&](int n) -> Result<void> { value = n; return {}; }));
    check(require(bindings.stats()).sources == 2, "Inactive branch was subscribed");
    b.set(8); check(calls == 1, "Inactive branch triggered work");
    choose.set(false); a.set(9);
    check(value == 8 && calls == 2, "Old branch subscription survived a switch");
    b.set(10); check(value == 10 && calls == 3, "New branch was not subscribed");
}
void derived() {
    Bindings bindings;
    Observable<int> input{1};
    auto twice = require(bindings.derive([&](ReadContext& c) { return c.read(input) * 2; }));
    auto thrice = require(bindings.derive([&](ReadContext& c) { return c.read(input) * 3; }));
    int applications = 0;
    require(bindings.bind([&](ReadContext& c) { return c.read(*twice) + c.read(*thrice); },
                          [&](int n) -> Result<void> {
                              ++applications;
                              check(n == input.get() * 5, "An effect observed a half-updated diamond");
                              return {};
                          }));
    input.set(4);
    check(applications == 2 && twice->get() == 8, "Derived values did not settle before effects");
}
void retire_effects() {
    Bindings graph;
    Observable<int> input{1};
    auto value = require(graph.derive([&](ReadContext& context) { return context.read(input) * 2; }));
    int shown = 0, applications = 0;
    require(graph.bind([&](ReadContext& context) { return context.read(*value); },
                       [&](int next) -> Result<void> { shown = next; ++applications; return {}; }));
    require(graph.begin_batch()); input.set(4);
    require(graph.retire_effects()); require(graph.retire_effects()); require(graph.end_batch());
    check(shown == 2 && applications == 1 && value->get() == 8,
          "Retiring presentation effects also stopped derived state or applied a pending effect");
    input.set(5);
    check(shown == 2 && value->get() == 10 && graph.available(), "A retired effect updated or closed its callback graph");
    auto rejected = graph.bind([](ReadContext&) { return 0; }, [](int) -> Result<void> { return {}; });
    check(!rejected && rejected.error().code == ii::core::ErrorCode::closed, "A retired graph accepted a new presentation effect");
    auto late_derived = require(graph.derive([&](ReadContext& context) { return context.read(input) + 1; }));
    check(late_derived->get() == 6, "Retiring effects closed derived value admission");
    bool wrong_thread = false;
    std::thread worker([&] { auto result = graph.retire_effects(); wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join();
    check(wrong_thread, "Another thread retired presentation effects");
    require(graph.close());

    Bindings reentrant;
    Observable<int> changing{0};
    int writes = 0;
    require(reentrant.bind([&](ReadContext& context) {
        auto current = context.read(changing);
        if (current == 1) require(reentrant.retire_effects());
        return current;
    }, [&](int) -> Result<void> { ++writes; return {}; }));
    changing.set(1);
    check(writes == 1, "An in-flight getter applied its effect after retirement");
}
void batches() {
    Bindings bindings;
    Observable<int> a{1}, b{2};
    int applications = 0, result = 0;
    require(bindings.bind([&](ReadContext& c) { return c.read(a) + c.read(b); },
                          [&](int n) -> Result<void> { ++applications; result = n; return {}; }));
    require(bindings.begin_batch()); require(bindings.begin_batch());
    a.set(3); b.set(4); require(bindings.end_batch());
    check(applications == 1, "Nested batch flushed too early");
    require(bindings.end_batch());
    check(applications == 2 && result == 7, "A batch did not coalesce effects");
    check(!bindings.end_batch(), "Unbalanced batch accepted");
}
void event_transactions() {
    struct Model final { Observable<int> x{0}, y{0}; };
    auto model = std::make_shared<Model>();
    auto graph = std::make_shared<Bindings>();
    std::vector<int> snapshots;
    require(graph->bind([&](ReadContext& context) { return context.read(model->x) + context.read(model->y); },
                        [&](int value) -> Result<void> { snapshots.push_back(value); return {}; }));
    auto input = scoped_function<int>(model, graph, [](const auto& owner, int sample) {
        owner->x.set(sample);
        owner->y.set(sample);
        return sample;
    });
    for (int sample = 1; sample <= 3; ++sample)
        check(input(sample) == sample, "An input callback lost its return value");
    std::cout << "EVENT_TRACE inputs=3 effects=" << snapshots.size()-1 << " values=";
    for (auto value : snapshots) std::cout << value << ',';
    std::cout << '\n';
    check((snapshots == std::vector<int>{0, 2, 4, 6}),
          "An input exposed intermediate model states to the binding graph");
    const auto trace = require(graph->stats());
    check(trace.invalidations == 6 && trace.coalesced == 3 && trace.flushes == 3 &&
              trace.applications == 4 && trace.high_watermark == 1 && trace.pending == 0 &&
              trace.batch_depth == 0 && !trace.draining,
          "Binding diagnostics did not describe the completed input transactions");
    std::vector<int> invoked;
    auto selected = returned_callback(model, graph, [&](ReadContext& context) {
        const auto version = context.read(model->x);
        if (version < 0) throw std::runtime_error("invalid callback version");
        return [&, version] { invoked.push_back(version); };
    }, [](const auto&, auto& handler) { handler(); });
    auto nested = callback(model, graph, [&](const auto& owner) {
        owner->x.set(7);
        selected();
        owner->y.set(7);
    });
    nested();
    check(invoked == std::vector<int>{7} && snapshots.back() == 14,
          "A nested callback used the previous viewmodel state");
    bool during_effect = false;
    require(graph->bind([&](ReadContext& context) { return context.read(model->y); },
                        [&](int value) -> Result<void> {
        if (value == 9) { model->x.set(9); selected(); during_effect = true; }
        return {};
    }));
    auto reenter = callback(model, graph, [](const auto& owner) { owner->y.set(9); });
    reenter();
    check(during_effect && invoked.back() == 9 && snapshots.back() == 18,
          "A callback invoked while effects drain used an obsolete derived handler");
    auto invalid = callback(model, graph, [&](const auto& owner) { owner->x.set(-1); selected(); });
    invalid();
    check(invoked.back() == 9 && !require(graph->failures()).empty(),
          "A failed nested callback switch did not preserve its last valid handler and diagnostic");
    model->x.set(9);
    check(require(graph->failures()).empty(), "A later valid callback switch did not recover");
    auto rejected = scoped_function<Result<bool>>(model, graph, [](const auto& owner) -> Result<bool> {
        owner->x.set(-2);
        return ii::core::fail(ii::core::ErrorCode::cancelled, "input cancelled");
    });
    const auto rejection = rejected();
    check(!rejection && rejection.error().code == ii::core::ErrorCode::cancelled &&
              !require(graph->failures()).empty(),
          "A binding failure replaced the input's explicit error");
    model->x.set(9);
    auto failing = callback(model, graph, [](const auto& owner) {
        owner->x.set(10);
        throw std::runtime_error("original input error");
    });
    bool original = false;
    try { failing(); } catch (const std::runtime_error& error) {
        original = std::string_view(error.what()) == "original input error";
    }
    check(original && snapshots.back() == 19 && require(graph->stats()).pending == 0,
          "An input exception lost its cause or left the graph batched");

    // A previously rejected presentation value can be reevaluated when an
    // unrelated method publishes its backend revision. It must not stop that
    // callback, while a newly introduced failure must still be reported.
    struct FailureModel final {
        Observable<int> value{0}, other{0}, revision{0};
        int callbacks = 0, shown = 0;
    };
    auto state = std::make_shared<FailureModel>(); auto retry_graph = std::make_shared<Bindings>();
    require(retry_graph->bind([&](ReadContext& context) {
        (void)context.read(state->revision); return context.read(state->value);
    }, [&](int value) -> Result<void> {
        if(value<0)return ii::core::fail(ii::core::ErrorCode::invalid_argument,value==-1?"bad option":"changed option");
        state->shown=value;return {};
    }));
    require(retry_graph->bind([&](ReadContext& context) {
        (void)context.read(state->revision); return context.read(state->other);
    }, [](int value) -> Result<void> {
        if(value<0)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"new option");return {};
    }));
    state->value.set(-1);
    check(require(retry_graph->failures()).size()==1 && state->shown==0,"A rejected value lost its diagnostic or last valid display");
    auto independent=callback(state,retry_graph,[](const auto& owner){++owner->callbacks;owner->revision.set(owner->revision.get()+1);});
    const auto previous_failed_flushes=require(retry_graph->stats()).failed_flushes;
    independent();
    check(state->callbacks==1 && state->shown==0 && require(retry_graph->failures()).size()==1 &&
          require(retry_graph->stats()).failed_flushes==previous_failed_flushes+1,
          "An existing rejected value interrupted a callback or disappeared from failure statistics");
    auto query=scoped_function<Result<int>>(state,retry_graph,[](const auto& owner) -> Result<int> {
        owner->revision.set(owner->revision.get()+1);return 42;
    });
    auto answer=query();
    check(answer && *answer==42 && require(retry_graph->failures()).size()==1,
          "A repeated binding failure changed an independent callback result");
    require(retry_graph->begin_batch());state->revision.set(state->revision.get()+1);
    auto manual=retry_graph->end_batch();
    check(!manual && manual.error().message=="bad option","An explicit batch hid its repeated failure");
    auto changed=callback(state,retry_graph,[](const auto& owner){owner->value.set(-2);});
    bool changed_error=false;try{changed();}catch(const std::runtime_error& error){changed_error=std::string_view(error.what())=="changed option";}
    check(changed_error,"A changed binding failure was hidden by the earlier diagnostic");
    state->value.set(-1);
    auto additional=callback(state,retry_graph,[](const auto& owner){owner->revision.set(owner->revision.get()+1);owner->other.set(-1);});
    bool new_error=false;try{additional();}catch(const std::runtime_error& error){new_error=std::string_view(error.what())=="new option";}
    check(new_error && require(retry_graph->failures()).size()==2,"A new binding failure was hidden by a repeated one");
    state->value.set(0);state->other.set(0);
    check(require(retry_graph->failures()).empty(),"Recovered values retained binding failures");
}
void fanout() {
    Bindings bindings;
    Observable<int> source{0};
    int changes = 0;
    for (int i = 0; i < 2048; ++i)
        require(bindings.bind([&](ReadContext& c) { return c.read(source); },
                              [&](int) -> Result<void> { ++changes; return {}; }));
    check(require(bindings.stats()).sources == 1, "Fanout allocated a Core observer per widget");
    source.set(1);
    check(changes == 4096, "Fanout missed native effects");
}
void errors() {
    Bindings bindings;
    Observable<int> divisor{2};
    int result = 0, healthy = 0;
    require(bindings.bind([&](ReadContext& c) {
        auto n = c.read(divisor);
        if (!n) throw std::domain_error("zero");
        return 10 / n;
    }, [&](int n) -> Result<void> { result = n; return {}; }));
    require(bindings.bind([&](ReadContext& c) { return c.read(divisor); },
                          [&](int n) -> Result<void> { healthy = n; return {}; }));
    divisor.set(0);
    check(result == 5 && healthy == 0 && require(bindings.failures()).size() == 1,
          "One getter failure blocked other effects or erased the last good value");
    divisor.set(5);
    check(result == 2 && require(bindings.failures()).empty(), "A valid source did not recover a failed binding");
    require(bindings.bind([&](ReadContext& c) { return c.read(divisor); },
                          [](int n) -> Result<void> {
        if (n < 0) return ii::core::fail(ii::core::ErrorCode::out_of_range, "negative");
        return {};
    }));
    divisor.set(-1); check(require(bindings.failures()).size() == 1, "Setter failure was hidden");
    divisor.set(2); check(require(bindings.failures()).empty(), "Setter failure did not recover");
}
void reentry() {
    Bindings bindings;
    Observable<int> input{1}, output{0};
    int value = 0;
    require(bindings.bind([&](ReadContext& c) {
        int n = c.read(input);
        if (n == 1) input.set(2);
        return n;
    }, [&](int n) -> Result<void> { value = n; return {}; }));
    check(value == 2, "A source changed during a getter left a stale initial value");
    require(bindings.bind([&](ReadContext& c) { return c.read(input); },
                          [&](int n) -> Result<void> { output.set(n); return {}; }));
    require(bindings.bind([&](ReadContext& c) { return c.read(output); },
                          [&](int n) -> Result<void> { value = n * 2; return {}; }));
    input.set(4); check(value == 8, "Setter reentry dropped an effect");
    Bindings closed;
    auto rejected = closed.bind([&](ReadContext&) { require(closed.close()); return 1; },
                               [](int) -> Result<void> { throw std::runtime_error("Closed getter applied"); });
    check(!rejected && rejected.error().code == ii::core::ErrorCode::closed, "A closed initializer returned a live binding");
    {
        auto dying = std::make_unique<Bindings>();
        Observable<int> source{0};
        require(dying->bind([&](ReadContext& c) { return c.read(source); },
                            [&](int n) -> Result<void> { if (n == 1) dying.reset(); return {}; }));
        require(dying->begin_batch());source.set(1);
        auto* raw = dying.get();
        require(raw->end_batch());
        check(!dying, "A graph retired during flush did not finish safely");
    }
    {
        Bindings deep;Observable<int> source{0};int shown=0;
        require(deep.bind([&](ReadContext& c) { return c.read(source); },
                          [&](int n) -> Result<void> { shown=n;return {}; }));
        const auto nest=[&](auto&& self,std::size_t depth)->void {
            if(depth) ii::core::ObservableReadScope::collect([](auto){},[&]{self(self,depth-1);});
            else source.set(1);
        };
        nest(nest,ii::core::ObservableReadScope::maximum_depth);
        check(shown==0&&!require(deep.failures()).empty(),
              "An evaluation scope failure was not retained as a binding diagnostic");
        source.set(2);
        check(shown==2&&require(deep.failures()).empty(),"A binding did not recover after a scope failure");
    }
}
void lifetime() {
    Observable<int> input{1};
    int value = 0;
    Bindings bindings;
    auto id = require(bindings.bind([&](ReadContext& c) { return c.read(input); },
                                   [&](int n) -> Result<void> { value = n; return {}; }));
    require(bindings.remove(id)); input.set(2);
    check(value == 1 && require(bindings.stats()).sources == 0, "Removal leaked a source lease");
    auto derived = require(bindings.derive([&](ReadContext& c) { return c.read(input); }));
    auto lease = derived->observe([](int, std::uint64_t) {});
    require(bindings.close()); require(bindings.close()); input.set(3);
    check(!lease.active() && !bindings.begin_batch(), "Close admitted work or leaked derived observers");
    bool rejected = false;
    try { (void)derived->get(); } catch (const std::logic_error&) { rejected = true; }
    check(rejected, "Derived output remained readable after activation closed");
    std::shared_ptr<ReadOnlyValue<int>> surviving;
    {
        Bindings temporary;
        surviving = require(temporary.derive([&](ReadContext& c) { return c.read(input); }));
    }
    rejected = false;
    try { (void)surviving->get(); } catch (const std::logic_error&) { rejected = true; }
    check(rejected, "Derived output outlived its activation graph");
}
void threading() {
    Bindings bindings;
    check(bindings.available(), "Owner could not enter its binding activation");
    bool rejected = false;
    std::thread worker([&] {
        const auto result = bindings.begin_batch();
        rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor;
        rejected &= !bindings.available();
    });
    worker.join();
    check(rejected && require(bindings.stats()).bindings == 0, "Foreign thread mutated the owner graph");
}
void callbacks() {
    struct Model final {
        int& destroyed;
        explicit Model(int& count) : destroyed(count) {}
        ~Model() { ++destroyed; }
        int calls = 0;
    };
    int destroyed = 0;
    auto model = std::make_shared<Model>(destroyed);
    auto activation = std::make_shared<Bindings>();
    auto call = callback(model, activation, [](const auto& owner, int delta) { owner->calls += delta; });
    check(model->calls == 0, "Method reference was eagerly invoked");
    call(2); check(model->calls == 2, "Admitted callback did not reuse its owner");
    std::weak_ptr<Model> weak = model;
    auto self_close = callback(model, activation, [&](const auto& owner) {
        require(activation->close());
        model.reset();
        check(!weak.expired() && destroyed == 0, "Closing destroyed an active native method receiver");
        call(5);
        check(owner->calls == 2, "Closed activation admitted a reentrant callback");
        ++owner->calls;
    });
    self_close();
    check(weak.expired() && destroyed == 1, "Saved callbacks retained a retired backend");
    call(3); self_close();
    check(destroyed == 1 && !activation->available(), "Late callback reopened or destroyed its owner twice");
}
void values() {
    check(add(2, 3) == 5 && multiply(2, 3) == 6 && divide(6, 2) == 3 && lvui::native::remainder(-5, 2) == -1,
          "Native arithmetic differs from constant folding");
    check(add(std::string_view("a"), "b") == "ab" && text(3.5) == "3.5", "Native String lost its value");
    check(equal(std::string("a"), "a") && !equal(true, false) && less(2, 3), "Native comparisons differ");
    check(choose(true, [] { return 7; }, []() -> int { throw std::runtime_error("Inactive branch ran"); }) == 7,
          "Conditional evaluated an inactive branch");
    check(index(std::vector<int>{4, 5}, 1) == 5 && integer(42.) == 42, "Native index or Int coercion failed");
    int rejected = 0;
    for (auto invalid : std::vector<std::function<void()>>{
            [] { (void)divide(1, 0); }, [] { (void)lvui::native::remainder(1, 0); },
            [] { (void)multiply(1e308, 1e308); }, [] { (void)integer(1.5); },
            [] { (void)index(std::vector<int>{1}, -1); }, [] { (void)index(std::vector<int>{1}, 2); }})
        try { invalid(); } catch (const std::exception&) { ++rejected; }
    check(rejected == 6, "Invalid runtime values were silently coerced");
    rejected = 0;
    const char* missing = nullptr;
    for (auto invalid : std::vector<std::function<void()>>{
            [&] { (void)text(missing); }, [&] { (void)add(missing, "x"); }, [&] { (void)equal(missing, "x"); }})
        try { invalid(); } catch (const std::invalid_argument&) { ++rejected; }
    check(rejected == 3, "Null String bypassed native validation");
    Bindings bindings;
    Observable<std::uint64_t> revision{0};
    int model = 1, output = 0;
    require(bindings.bind([&](ReadContext& c) { return read(c, model, revision); },
                          [&](int n) -> Result<void> { output = n; return {}; }));
    check(invoke(revision, [&] { model = 4; return 7; }) == 7 && output == 4,
          "Bound invocation did not publish an ordinary native member");
    try { invoke(revision, [&] { model = 8; throw std::runtime_error("method"); }); }
    catch (const std::runtime_error& error) { check(std::string_view(error.what()) == "method", "Publication replaced method failure"); }
    check(output == 8, "A throwing method hid its committed public changes");
}
void cycle() {
    Bindings bindings;
    Observable<int> source{0};
    auto result = bindings.bind([&](ReadContext& c) { return c.read(source); },
                                [&](int n) -> Result<void> { source.set(n + 1); return {}; });
    check(!result && result.error().code == ii::core::ErrorCode::cycle, "Unbounded initializer was accepted");
    check(require(bindings.stats()).bindings == 0 && require(bindings.stats()).sources == 0,
          "Failed initializer retained native connections");
}
void subscription_rollback() {
    struct Source final {
        Observable<int> value{8};
        bool unavailable = true;
        const int& get() const { return value.get(); }
        std::uint64_t revision() const { return value.revision(); }
        ii::core::ObservableSubscription observe(std::function<void(const int&, std::uint64_t)> changed) {
            if (unavailable) throw std::runtime_error("Source not ready");
            return value.observe(std::move(changed));
        }
    } incoming;
    Bindings bindings;
    Observable<bool> choose{true};
    Observable<int> previous{2};
    int shown = 0, calls = 0;
    require(bindings.bind([&](ReadContext& c) { ++calls; return c.read(choose) ? c.read(previous) : c.read(incoming); },
                          [&](int n) -> Result<void> { shown = n; return {}; }));
    choose.set(false);
    auto failures = require(bindings.failures());
    check(shown == 2 && failures.size() == 1 && failures[0].error.cause &&
          failures[0].error.message.find("Source not ready") != std::string::npos,
          "Subscription failure lost its diagnostic or committed a partial update");
    check(require(bindings.stats()).sources == 2, "Failed subscription changed the old source graph");
    incoming.unavailable = false;
    previous.set(3);
    check(shown == 8 && require(bindings.failures()).empty(), "Preserved dependency did not retry the failed switch");
    const auto settled = calls;
    previous.set(4);
    check(calls == settled, "Successful retry retained the inactive dependency");
    incoming.value.set(9);
    check(shown == 9, "Successful retry did not publish the new source");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "selective";
        if (name == "selective") selective(); else if (name == "readonly") readonly();
        else if (name == "readonly_members") readonly_members(); else if (name == "readonly_reentry") readonly_reentry();
        else if (name == "readonly_owners") readonly_owners();else if (name == "readonly_references") readonly_references();
        else if (name == "branches") branches();
        else if (name == "derived") derived(); else if (name == "batches") batches();
        else if (name == "event_transactions") event_transactions();
        else if (name == "fanout") fanout(); else if (name == "errors") errors();
        else if (name == "reentry") reentry(); else if (name == "lifetime") lifetime();
        else if (name == "threading") threading(); else if (name == "values") values();
        else if (name == "cycle") cycle(); else if (name == "subscription_rollback") subscription_rollback();
        else if (name == "callbacks") callbacks();
        else if (name == "retire_effects") retire_effects();
        else throw std::runtime_error("Unknown native binding test");
        std::cout << "LVUI native bindings " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
