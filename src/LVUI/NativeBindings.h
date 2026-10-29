#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace lvui::native {
using BindingId = std::uint64_t;
namespace detail {
struct BindingState;
using SourceRead=ii::core::ObservableRead;
template<class T>
using StoredValue = std::conditional_t<std::convertible_to<T, std::string_view>, std::string,
                                       std::remove_cvref_t<T>>;
template<class T> StoredValue<T> snapshot(T&& value) {
    if constexpr (std::convertible_to<T, std::string_view>) {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
            if (!value) throw std::invalid_argument("A native String cannot be null");
        return std::string(std::string_view(value));
    } else return std::forward<T>(value);
}
}

// The generated C++ getter explicitly routes reads through this context. There
// is no source evaluator, process-global bus or frame-by-frame polling.
class ReadContext final {
public:
    static constexpr std::size_t maximum_retained_values=4096;
    template<class T>void retain(const T&) {}
    template<class T>void retain(const std::shared_ptr<T>& value) {
        if(!value.use_count())return;
        // Alias only the control block: function pointers and arrays need no
        // conversion to an object pointer to preserve their ownership.
        std::shared_ptr<const void> owner(value,nullptr);
        for(const auto& previous:owners_)
            if(!owner.owner_before(previous)&&!previous.owner_before(owner))return;
        if(owners_.size()==maximum_retained_values)throw std::length_error("Native read ownership capacity exhausted");
        owners_.push_back(std::move(owner));
    }
    template<class Source>
        requires requires(Source& source) {
            source.get(); source.revision();
            source.observe([](const auto&, std::uint64_t) {});
        }
    decltype(auto) read(Source& source) {
        const auto revision=source.revision();
        decltype(auto) value=ii::core::ObservableReadScope::collect([this](ii::core::ObservableRead read){retain(read.owner);retain(read.value_owner);},[&]() -> decltype(auto){return source.get();});
        retain(value);
        if constexpr(requires{{source.read_dependency()} -> std::same_as<ii::core::ObservableRead>;}){
            auto read=source.read_dependency();read.revision=revision;track(std::move(read));
        }else track({std::addressof(source), revision,
               [&source](std::function<void()> changed) {
                   return source.observe([changed = std::move(changed)](const auto&, std::uint64_t) { changed(); });
               }, [&source] { return source.revision(); }, {}, {}});
        if constexpr(std::is_rvalue_reference_v<decltype(value)>)return std::move(value);
        else return value;
    }
private:
    friend struct detail::BindingState;
    friend class Methods;
    ReadContext() = default;
    void track(detail::SourceRead);
    std::vector<detail::SourceRead> reads_;
    std::vector<std::shared_ptr<const void>> owners_;
};

template<class T> class ReadOnlyValue final {
public:
    [[nodiscard]] const T& get() const { check(); return value_->get(); }
    [[nodiscard]] std::uint64_t revision() const { check(); return value_->revision(); }
    [[nodiscard]] ii::core::ObservableSubscription observe(
            std::function<void(const T&, std::uint64_t)> changed, bool initial = false) const {
        check();
        return value_->observe(std::move(changed), initial);
    }
private:
    friend class Bindings;
    void check() const {
        auto lifetime = lifetime_.lock();
        if (!lifetime || !lifetime->load() || !value_)
            throw std::logic_error("Native derived value is closed");
    }
    void assign(const T& value) {
        if (value_) value_->set(value); else value_.emplace(value);
    }
    void close() { value_.reset(); }
    mutable std::optional<ii::core::Observable<T>> value_;
    std::weak_ptr<const std::atomic_bool> lifetime_;
};

struct BindingStats final {
    std::size_t bindings = 0, sources = 0, pending = 0;
    std::uint64_t evaluations = 0, applications = 0;
};
struct BindingFailure final { BindingId binding; ii::core::Error error; };

// One owner-thread graph per activation. Effects share each Core observation;
// derived values settle before effects and a batch coalesces model transactions.
class Bindings final {
public:
    static constexpr std::size_t maximum_bindings = 16384, maximum_sources = 4096,
                                 maximum_evaluations_per_flush = 65536;
    Bindings();
    ~Bindings();
    Bindings(const Bindings&) = delete;
    Bindings& operator=(const Bindings&) = delete;
    Bindings(Bindings&&) = delete;
    Bindings& operator=(Bindings&&) = delete;

    template<class Getter, class Setter>
    [[nodiscard]] ii::core::Result<BindingId> bind(Getter getter, Setter setter) {
        return add(preparer(std::move(getter), std::move(setter)), false, {});
    }
    template<class Getter>
    [[nodiscard]] auto derive(Getter getter)
        -> ii::core::Result<std::shared_ptr<ReadOnlyValue<detail::StoredValue<std::invoke_result_t<Getter&, ReadContext&>>>>> {
        using T = detail::StoredValue<std::invoke_result_t<Getter&, ReadContext&>>;
        auto value = std::make_shared<ReadOnlyValue<T>>();
        value->lifetime_ = lifetime();
        auto added = add(preparer(std::move(getter), [value](const T& next) -> ii::core::Result<void> {
            value->assign(next);
            return {};
        }), true, [value] { value->close(); });
        if (!added) return std::unexpected(added.error());
        return value;
    }
    [[nodiscard]] ii::core::Result<void> remove(BindingId);
    [[nodiscard]] ii::core::Result<void> begin_batch();
    [[nodiscard]] ii::core::Result<void> end_batch();
    [[nodiscard]] ii::core::Result<void> flush();
    [[nodiscard]] ii::core::Result<void> close();
    // A weak callback may check admission without touching model storage.
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] ii::core::Result<BindingStats> stats() const;
    [[nodiscard]] ii::core::Result<std::vector<BindingFailure>> failures() const;
private:
    using Update = std::function<ii::core::Result<void>()>;
    using Prepare = std::function<ii::core::Result<Update>(ReadContext&)>;
    template<class Getter, class Setter> static Prepare preparer(Getter getter, Setter setter) {
        using T = detail::StoredValue<std::invoke_result_t<Getter&, ReadContext&>>;
        static_assert(std::copy_constructible<Getter> && std::copy_constructible<Setter>);
        auto previous = std::make_shared<std::shared_ptr<const T>>();
        return [getter = std::move(getter), setter = std::move(setter), previous](ReadContext& context) mutable
            -> ii::core::Result<Update> {
            auto next = std::make_shared<T>(detail::snapshot(std::invoke(getter, context)));
            if constexpr (std::equality_comparable<T>)
                if (*previous && **previous == *next) return Update{};
            return Update{[setter, previous, next]() mutable -> ii::core::Result<void> {
                auto applied = std::invoke(setter, std::as_const(*next));
                if (applied) *previous = next;
                return applied;
            }};
        };
    }
    [[nodiscard]] ii::core::Result<BindingId> add(Prepare, bool derived, std::function<void()> close);
    [[nodiscard]] std::shared_ptr<const std::atomic_bool> lifetime() const;
    std::shared_ptr<detail::BindingState> state_;
};
} // namespace lvui::native
