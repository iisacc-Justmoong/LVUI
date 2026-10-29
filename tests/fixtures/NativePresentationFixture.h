#pragma once
#include <LVRS/StateModel.h>
#include <LVRS/ProgressModel.h>
struct NativePresentationBackend final {
    std::shared_ptr<lvrs::StateModel> state=std::make_shared<lvrs::StateModel>();
    lvrs::ProgressModel progress;
    ii::core::Observable<int> file_revision{0};
    int count=0;
    NativePresentationBackend(){
        require(state->set_key("Generation"));require(state->set_display_name("Native state"));
        require(state->apply_patch({{"minimumValue",ii::core::Value(10)},{"maximumValue",ii::core::Value(110)},{"startValue",ii::core::Value(30)},{"currentValue",ii::core::Value(80)}}));
        require(progress.set_state_model(state));
    }
    void advance(){require(state->set_value("currentValue",ii::core::Value(++count+80)));}
    static void require(ii::core::Result<bool> result){if(!result)throw std::runtime_error(result.error().message);}
};
