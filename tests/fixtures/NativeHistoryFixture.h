#pragma once
#include <LVRS/ModelUndoStack.h>
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeHistoryBackend final {
    lvrs::ModelUndoStack history;
    ii::core::Observable<std::int64_t> current{0},file_revision{0};
    template<class T>static T take(ii::core::Result<T> result){
        if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}
        return std::move(*result);
    }
    void remember(){take(history.push_snapshot(ii::core::Value(current.get())));current.set(current.get()+1);}
    void undo(){apply(take(history.take_undo_snapshot(ii::core::Value(current.get()))));}
    void redo(){apply(take(history.take_redo_snapshot(ii::core::Value(current.get()))));}
    void clear(){take(history.clear());}
    void apply(lvrs::SnapshotTransfer result){
        if(result.snapshot)current.set(take(result.snapshot->get<std::int64_t>()).get());
        if(result.observation_error){if(result.observation_error->cause)std::rethrow_exception(result.observation_error->cause);throw std::runtime_error(result.observation_error->message);}
    }
};
