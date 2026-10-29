#pragma once
#include <LVUI/NativeValues.h>
#include <array>

namespace lvui::native {
// Native getters remain ordinary C++. Core records only Observable reads made
// by this synchronous call, while the existing graph owns updates and errors.
class Methods final {
    template<class Receiver,class Selector>class Reference final {
        friend class Methods;
        using Object=std::remove_reference_t<Receiver>;
        Object* receiver_;
        Selector selector_;
        std::vector<std::shared_ptr<const void>> owners_;
    public:
        Reference(Object* receiver,Selector selector):receiver_(receiver),selector_(std::move(selector)){}
        template<class... Args>requires std::invocable<Selector&,Receiver,Args...>
        decltype(auto) operator()(Args&&... args){
            return std::invoke(selector_,std::forward<Receiver>(*receiver_),std::forward<Args>(args)...);
        }
        template<class... Args>requires std::invocable<const Selector&,Receiver,Args...>
        decltype(auto) operator()(Args&&... args)const{
            return std::invoke(selector_,std::forward<Receiver>(*receiver_),std::forward<Args>(args)...);
        }
    };
    template<class Factory,class Selector>static decltype(auto) with_receiver(ReadContext& context,Factory& factory,Selector selector){
        using Returned=std::invoke_result_t<Factory&,ReadContext&>;
        static_assert(!std::is_void_v<Returned>,"A deferred native method requires an object receiver");
        return evaluate(context,[&]() -> decltype(auto){
            if constexpr(std::is_reference_v<Returned>)
                return member(context,std::invoke(factory,context),std::move(selector));
            else {
                using Object=std::remove_reference_t<Returned>;
                // Construct prvalues directly, including immovable C++ objects.
                // Lvalue/reference receivers keep their original identity.
                auto owned=std::shared_ptr<Object>(new Object(std::invoke(factory,context)));
                context.retain(owned);
                return member(context,std::move(*owned),std::move(selector));
            }
        });
    }
    template<class Factory,class Selector>static auto select_receiver(ReadContext& context,Factory& factory,Selector selector){
        return with_receiver(context,factory,[&](auto&& receiver){
            return Reference<decltype(receiver),Selector>(std::addressof(receiver),std::move(selector));
        });
    }
    struct InvokeField final {
        template<class Handler,class... Args>requires std::invocable<Handler,Args...>
        decltype(auto) operator()(Handler&& handler,Args&&... args)const{
            return std::invoke(std::forward<Handler>(handler),std::forward<Args>(args)...);
        }
    };
    struct NoPublication final {void operator()(ReadContext&)const{}};
    template<class Value>static auto borrow_callable(Value&& value){
        detail::require_callable(value);
        return Reference<Value&&,InvokeField>(std::addressof(value),InvokeField{});
    }
    template<bool Snapshot=false,class Value>static auto select_callable(ReadContext& context,Value&& value){
        using T=std::remove_cvref_t<Value>;
        if constexpr(ObservableSource<std::remove_reference_t<Value>>)
            return select_callable<true>(context,context.read(value));
        else if constexpr(ReadableSource<std::remove_reference_t<Value>>)
            return select_callable<true>(context,retained_initial(context,value));
        else if constexpr(detail::member_reference<T>)return borrow_callable(value.get());
        else if constexpr((std::is_pointer_v<T>&&!std::is_function_v<std::remove_pointer_t<T>>)||
                          requires{value.get();*value;bool(value);}){
            if(!value)throw std::invalid_argument("Native callable field requires a live handle");
            context.retain(value);return borrow_callable(*value);
        }else if constexpr(Snapshot||(std::is_pointer_v<T>&&std::is_function_v<std::remove_pointer_t<T>>)){
            // Published values are selected versions. Handles above retain the
            // actual callable, so move-only handlers need no clone protocol.
            auto owned=std::make_shared<T>(std::forward<Value>(value));context.retain(owned);
            return borrow_callable(*owned);
        }else return borrow_callable(std::forward<Value>(value));
    }
    template<class Factory,class Selector,class Field,class Publication>
    static auto select_member(ReadContext& context,Factory& factory,Selector selector,Field field,Publication publication){
        return with_receiver(context,factory,[&](auto&& receiver){
            if constexpr(std::invocable<Field&,decltype(receiver)>){
                using Selected=std::invoke_result_t<Field&,decltype(receiver)>;
                // Only observable fields publish their own changes. Readable
                // get/revision wrappers without observe still need the owner.
                if constexpr(!ObservableSource<std::remove_reference_t<Selected>>)std::invoke(publication,context);
                if constexpr(std::is_reference_v<Selected>)
                    return select_callable(context,std::invoke(field,std::forward<decltype(receiver)>(receiver)));
                else {
                    // A C++ accessor may compute a prvalue rather than expose
                    // a field reference. Own it before selecting its callable.
                    auto owned=std::shared_ptr<Selected>(new Selected(std::invoke(field,std::forward<decltype(receiver)>(receiver))));
                    context.retain(owned);return select_callable(context,std::move(*owned));
                }
            }else return Reference<decltype(receiver),Selector>(std::addressof(receiver),std::move(selector));
        });
    }
    template<class Selection>static auto capture_reference(ReadContext* outer,Selection selection){
        ReadContext selected;
        try{
            auto result=std::invoke(selection,selected);result.owners_=std::move(selected.owners_);
            if(outer)for(auto& read:selected.reads_)outer->track(std::move(read));
            return result;
        }catch(...){
            // Failed selections still subscribe to values that can recover.
            if(outer)for(auto& read:selected.reads_)outer->track(std::move(read));
            throw;
        }
    }
    template<class Factory>static auto select_factory(ReadContext& context,Factory& factory){
        using Returned=std::invoke_result_t<Factory&,ReadContext&>;
        static_assert(!std::is_void_v<Returned>,"A native callable factory cannot return void");
        return evaluate(context,[&]{
            if constexpr(std::is_lvalue_reference_v<Returned>)
                return select_callable(context,std::invoke(factory,context));
            else {
                using Handler=std::remove_reference_t<Returned>;
                // Stored factory results are invoked as lvalues. Preserve the
                // existing value/rvalue ownership and guaranteed copy elision.
                auto owned=std::shared_ptr<Handler>(new Handler(std::invoke(factory,context)));
                context.retain(owned);return select_callable(context,*owned);
            }
        });
    }
    template<std::size_t Index,auto Selected,class Probe>static consteval auto select_arguments(){
        if constexpr(Index==Selected.size())return Selected;
        else {
            constexpr auto candidate=[]{auto next=Selected;next[Index]=true;return next;}();
            if constexpr(Probe{}.template operator()<candidate>())return select_arguments<Index+1,candidate,Probe>();
            else return select_arguments<Index+1,Selected,Probe>();
        }
    }
public:
    template<class Factory,class Marker>static auto inferred_result(ReadContext& context,Factory factory,Marker marker){
        return capture_reference(&context,[&](ReadContext& selected){
            using Returned=std::invoke_result_t<Factory&,ReadContext&>;
            if constexpr(Marker::template is_alias<Returned>)
                return evaluate(selected,[&]{return marker.alias(std::invoke(factory,selected));});
            else if constexpr(Marker::template is_callable<Returned>)
                return marker.template callable<Returned>(select_factory(selected,factory));
            else return evaluate(selected,[&]{return marker.plain(value_copy(std::invoke(factory,selected)));});
        });
    }
    template<class Factory,class Marker>static auto inferred_result(Factory factory,Marker marker){
        ReadContext ignored;
        return inferred_result(ignored,std::move(factory),std::move(marker));
    }
private:
    template<class Source,class Publication,class Marker>
    static auto inferred_source_value(ReadContext& context,Source&& source,Publication publication,Marker marker){
        if constexpr(!ObservableSource<std::remove_reference_t<Source>>)std::invoke(publication,context);
        decltype(auto) value=receiver_value(context,std::forward<Source>(source));
        if constexpr(Marker::template is_alias<decltype(value)>)return marker.alias(value);
        else if constexpr(Marker::template is_callable<decltype(value)>)
            return marker.template callable<decltype(value)>(select_callable(context,std::forward<decltype(value)>(value)));
        else return marker.plain(value_copy(std::forward<decltype(value)>(value)));
    }
    template<class Receiver,class Field,class Call,class Pointer,class Publication,class Marker>
    static auto inferred_native_member(ReadContext& context,Receiver&& receiver,Field field,Call call,[[maybe_unused]] Pointer pointer,Publication publication,Marker marker){
        if constexpr(std::invocable<Field&,Receiver>){
            decltype(auto) source=std::invoke(field,std::forward<Receiver>(receiver));
            return inferred_source_value(context,std::forward<decltype(source)>(source),std::move(publication),std::move(marker));
        }else{
            static_assert(std::invocable<Pointer&,Receiver>,"An overloaded native method requires an explicit callable signature");
            using Member=std::invoke_result_t<Pointer&,Receiver>;
            return marker.template method<Member>(Reference<Receiver&&,Call>(std::addressof(receiver),std::move(call)));
        }
    }
public:
    template<class Factory,class Field,class Call,class Pointer,class Publication,class Marker>
    static auto inferred_member(ReadContext& context,Factory factory,Field field,Call call,Pointer pointer,Publication publication,Marker marker){
        return capture_reference(&context,[&](ReadContext& selected){
            return with_receiver(selected,factory,[&](auto&& receiver){
                return inferred_native_member(selected,std::forward<decltype(receiver)>(receiver),std::move(field),std::move(call),std::move(pointer),std::move(publication),std::move(marker));
            });
        });
    }
    template<class Factory,class Field,class Call,class Pointer,class Marker>
    static auto inferred_member(Factory factory,Field field,Call call,Pointer pointer,Marker marker){
        ReadContext ignored;
        return inferred_member(ignored,std::move(factory),std::move(field),std::move(call),std::move(pointer),NoPublication{},std::move(marker));
    }
    template<class Factory,class Field,class Call,class Pointer,class Publication,class Marker>
    static auto inferred_named_member(ReadContext& context,Factory factory,std::string_view key,Field field,Call call,Pointer pointer,Publication publication,Marker marker){
        return capture_reference(&context,[&](ReadContext& selected){
            return with_receiver(selected,factory,[&](auto&& receiver){
                if constexpr(detail::NamedData<decltype(receiver)>){
                    auto source=index(receiver,key);
                    return inferred_source_value(selected,std::move(source),std::move(publication),std::move(marker));
                }else return inferred_native_member(selected,std::forward<decltype(receiver)>(receiver),std::move(field),std::move(call),std::move(pointer),std::move(publication),std::move(marker));
            });
        });
    }
    template<class Factory,class Field,class Call,class Pointer,class Marker>
    static auto inferred_named_member(Factory factory,std::string_view key,Field field,Call call,Pointer pointer,Marker marker){
        ReadContext ignored;
        return inferred_named_member(ignored,std::move(factory),key,std::move(field),std::move(call),std::move(pointer),NoPublication{},std::move(marker));
    }
    template<std::size_t Count,class Probe>static consteval auto arguments(Probe){
        constexpr std::array<bool,Count> initial{};
        static_assert(Probe{}.template operator()<initial>(),"No accessible native method accepts these factory arguments");
        return select_arguments<0,initial,Probe>();
    }
    template<bool Retained,class Factory>static decltype(auto) argument_value(ReadContext& context,Factory& factory){
        if constexpr(Retained)return argument(context,factory);
        else return std::invoke(factory,context);
    }
    template<class Factory>static decltype(auto) argument(ReadContext& context,Factory factory){
        using Returned=std::invoke_result_t<Factory&,ReadContext&>;
        static_assert(!std::is_void_v<Returned>,"A native factory argument cannot be void");
        return evaluate(context,[&]() -> decltype(auto){
            if constexpr(std::is_reference_v<Returned>){
                decltype(auto) selected=std::invoke(factory,context);context.retain(selected);
                return std::forward<Returned>(selected);
            }else{
                auto owned=std::shared_ptr<Returned>(new Returned(std::invoke(factory,context)));
                context.retain(owned);return std::move(*owned);
            }
        });
    }
    template<class Factory>static auto callable(ReadContext& context,Factory factory){
        return capture_reference(&context,[&](ReadContext& selected){return select_factory(selected,factory);});
    }
    template<class Factory>static auto callable(Factory factory){
        return capture_reference(nullptr,[&](ReadContext& selected){return select_factory(selected,factory);});
    }
    template<class Value>static decltype(auto) receiver_value(ReadContext& context,Value&& value){
        if constexpr(ObservableSource<std::remove_reference_t<Value>>)return context.read(value);
        else if constexpr(ReadableSource<std::remove_reference_t<Value>>)return retained_initial(context,value);
        else {context.retain(value);return std::forward<Value>(value);}
    }
    // Preserve intermediate prvalues before exposing their child references.
    template<class Factory,class Selector>static decltype(auto) receiver_member(ReadContext& context,Factory factory,Selector selector){
        return with_receiver(context,factory,std::move(selector));
    }
    template<class Factory,class Selector>static decltype(auto) receiver_member(ReadContext& context,Factory factory,std::string_view key,Selector selector){
        return with_receiver(context,factory,[&](auto&& object)->decltype(auto){
            return named_member(context,std::forward<decltype(object)>(object),key,std::move(selector));
        });
    }
    template<class Factory,class Index>static decltype(auto) receiver_index(ReadContext& context,Factory factory,Index index){
        return with_receiver(context,factory,[&](auto&& range) -> decltype(auto){
            const auto value=integer(std::invoke(index,context));
            if(value<0||static_cast<std::uint64_t>(value)>=std::ranges::size(range))throw std::out_of_range("Deferred method receiver index is outside the collection");
            return std::forward<decltype(range)>(range)[static_cast<std::size_t>(value)];
        });
    }
    // Select the receiver now without invoking its method. Parent leases also
    // protect a borrowed subobject after its selected shared parent changes.
    template<class Factory,class Selector>static auto reference(ReadContext& context,Factory factory,Selector selector){
        return capture_reference(&context,[&](ReadContext& selected){return select_receiver(selected,factory,std::move(selector));});
    }
    template<class Factory,class Selector>static auto reference(Factory factory,Selector selector){
        return capture_reference(nullptr,[&](ReadContext& selected){return select_receiver(selected,factory,std::move(selector));});
    }
    // C++ decides whether the public member is a method or a callable field.
    // Neither selecting a field nor binding a method invokes the operation.
    template<class Factory,class Selector,class Field,class Publication=NoPublication>
    static auto reference(ReadContext& context,Factory factory,Selector selector,Field field,Publication publication={}){
        return capture_reference(&context,[&](ReadContext& selected){return select_member(selected,factory,std::move(selector),std::move(field),std::move(publication));});
    }
    template<class Factory,class Selector,class Field,class Publication=NoPublication>
    static auto reference(Factory factory,Selector selector,Field field,Publication publication={}){
        return capture_reference(nullptr,[&](ReadContext& selected){return select_member(selected,factory,std::move(selector),std::move(field),std::move(publication));});
    }
    template<class Function>static decltype(auto) evaluate(ReadContext& context,Function&& function){
        return ii::core::ObservableReadScope::collect([&](ii::core::ObservableRead read){context.track(std::move(read));},std::forward<Function>(function));
    }
    template<class Function>static decltype(auto) evaluate(Function&& function){
        ReadContext context;using Result=std::invoke_result_t<Function&>;
        if constexpr(std::convertible_to<Result,std::string_view>)return std::string(lvui::native::string_view(evaluate(context,std::forward<Function>(function))));
        else return evaluate(context,std::forward<Function>(function));
    }
    template<class Object,class Selector,class... Args>
    static decltype(auto) query(ReadContext& context,Object&& object,Selector selector,Args&&... args){
        return evaluate(context,[&]() -> decltype(auto){
            return member(context,std::forward<Object>(object),[&](auto&& receiver) -> decltype(auto){
                return std::invoke(selector,std::forward<decltype(receiver)>(receiver),std::forward<Args>(args)...);
            });
        });
    }
    template<class Object,class Selector,class... Args>
    static decltype(auto) query(Object&& object,Selector selector,Args&&... args){
        ReadContext context;
        using Result=decltype(query(context,std::forward<Object>(object),std::move(selector),std::forward<Args>(args)...));
        if constexpr(std::convertible_to<Result,std::string_view>)return std::string(lvui::native::string_view(query(context,std::forward<Object>(object),std::move(selector),std::forward<Args>(args)...)));
        else return query(context,std::forward<Object>(object),std::move(selector),std::forward<Args>(args)...);
    }
};
}
