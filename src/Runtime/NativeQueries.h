#pragma once
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lvui::native {
// Preserve the port's typed transport error (including its cause) for the
// binding/callback diagnostic instead of conflating it with a domain reply.
template<class Error> class QueryFailure final : public std::runtime_error {
public:
    explicit QueryFailure(Error error)
        : std::runtime_error("iiSDK query delivery failed"), error_(std::move(error)) {}
    [[nodiscard]] const Error& error() const noexcept { return error_; }
private:
    Error error_;
};

template<class Port, class Request>
auto send_query(Port& port, Request&& request) {
    auto reply = port.send(std::forward<Request>(request));
    using Reply = std::remove_cvref_t<decltype(reply)>;
    if (!reply) throw QueryFailure<typename Reply::error_type>(std::move(reply.error()));
    if constexpr (!std::is_void_v<typename Reply::value_type>) return std::move(*reply);
}
} // namespace lvui::native
