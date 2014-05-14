
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "react/detail/Defs.h"

#include "react/detail/IReactiveNode.h"
#include "react/detail/ObserverBase.h"
#include "react/detail/graph/ObserverNodes.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename E>
class Events;

enum class EventToken;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observer
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class Observer
{
private:
    using SubjectT  = REACT_IMPL::NodeBasePtrT<D>;
    using NodeT     = REACT_IMPL::IObserver;

public:
    Observer() :
        nodePtr_{ nullptr }
    {}

    Observer(const Observer&) = delete;

    Observer(Observer&& other) :
        nodePtr_{ std::move(other.nodePtr_) },
        subject_{ std::move(other.subject_) }
    {}

    Observer(NodeT* nodePtr, const SubjectT& subject) :
        nodePtr_{ nodePtr },
        subject_{ subject }
    {}

    bool IsValid() const
    {
        return nodePtr_ != nullptr;
    }

    void Detach()
    {
        REACT_ASSERT(IsValid(), "Detach on invalid Observer.");
        REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().Unregister(nodePtr_);
    }

private:
    // Ownership managed by registry
    NodeT*      nodePtr_;

    // While the observer handle exists, the subject is not destroyed
    SubjectT    subject_;    
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ScopedObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ScopedObserver
{
public:
    ScopedObserver(Observer<D>&& obs) :
        obs_{ std::move(obs) }
    {}

    ~ScopedObserver()
    {
        if (obs_.IsValid())
            obs_.Detach();
    }

private:
    Observer<D>     obs_;    
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Signals
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename FIn,
    typename S
>
auto Observe(const Signal<D,S>& subject, FIn&& func)
    -> Observer<D>
{
    using F = std::decay<FIn>::type;
    using TNode = REACT_IMPL::SignalObserverNode<D,S,F>;

    auto* obs = REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().
        template Register<TNode>(subject, std::forward<FIn>(func));

    return Observer<D>{ obs, subject.NodePtr() };
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename FIn,
    typename E,
    class = std::enable_if<
        ! std::is_same<E,EventToken>::value>::type
>
auto Observe(const Events<D,E>& subject, FIn&& func)
    -> Observer<D>
{
    using F = std::decay<FIn>::type;
    using TNode = REACT_IMPL::EventObserverNode<D,E,F>;

    auto* raw = REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().
        template Register<TNode>(subject, std::forward<FIn>(func));

    return Observer<D>(raw, subject.NodePtr());
}

template
<
    typename D,
    typename FIn
>
auto Observe(const Events<D,EventToken>& subject, FIn&& func)
    -> Observer<D>
{
    using F = std::decay<FIn>::type;

    struct Wrapper_
    {
        Wrapper_(FIn&& func) : MyFunc{ std::forward<FIn>(func) } {}
        Wrapper_(const Wrapper_& other) = default;
        Wrapper_(Wrapper_&& other) : MyFunc{ std::move(other.MyFunc) } {}

        void operator()(EventToken) { MyFunc(); }

        F MyFunc;
    };

    Wrapper_ wrapper{ std::forward<FIn>(func) };

    using TNode = REACT_IMPL::EventObserverNode<D,EventToken,Wrapper_>;

    auto* raw = REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().
        template Register<TNode>(subject, std::move(wrapper));

    return Observer<D>(raw, subject.NodePtr());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename FIn,
    typename E,
    typename TDepValue1,
    typename ... TDepValues,
    class = std::enable_if<
        ! std::is_same<E,EventToken>::value>::type
>
auto Observe(const Events<D,E>& subject, FIn&& func,
             const Signal<D,TDepValue1>& dep1, const Signal<D,TDepValues>& ... deps)
    -> Observer<D>
{
    using F = std::decay<FIn>::type;
    using TNode = REACT_IMPL::SyncedObserverNode<D,E,F,TDepValue1,TDepValues ...>;

    auto* raw = REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().
        template Register<TNode>(subject, std::forward<FIn>(func), dep1, deps ...);

    return Observer<D>(raw, subject.NodePtr());
}

// Synced
template
<
    typename D,
    typename FIn,
    typename TDepValue1,
    typename ... TDepValues
>
auto Observe(const Events<D,EventToken>& subject, FIn&& func,
             const Signal<D,TDepValue1>& dep1, const Signal<D,TDepValues>& ... deps)
    -> Observer<D>
{
    using F = std::decay<FIn>::type;

    struct Wrapper_
    {
        Wrapper_(FIn&& func) : MyFunc{ std::forward<FIn>(func) } {}
        Wrapper_(const Wrapper_& other) = default;
        Wrapper_(Wrapper_&& other) : MyFunc{ std::move(other.MyFunc) } {}

        void operator()(EventToken, const TDepValue1& arg1, const TDepValues& ... args)
        {
            MyFunc(arg1, args ...);
        }

        F MyFunc;
    };

    using TNode = REACT_IMPL::SyncedObserverNode<D,EventToken,Wrapper_,TDepValue1,TDepValues ...>;

    Wrapper_ wrapper{ std::forward<FIn>(func) };

    auto* raw = REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().
        template Register<TNode>(subject, std::move(wrapper), dep1, deps ...);

    return Observer<D>(raw, subject.NodePtr());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DetachAllObservers
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TSubject>
void DetachAllObservers(const TSubject& subject)
{
    using D = typename TSubject::DomainT;

    REACT_IMPL::DomainSpecificObserverRegistry<D>::Instance().UnregisterFrom(
        subject.NodePtr().get());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DetachThisObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
inline void DetachThisObserver()
{
    REACT_IMPL::current_observer_state_::shouldDetach = true;
}

/******************************************/ REACT_END /******************************************/