#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <variant>
#include <vector>

struct NativeOwnedListItemFormModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::form};
    ii::core::Observable<std::string> title{"Title"},notes{"Notes"},label2{"Notes"};
    ii::core::Observable<bool> read_only2{false};
    std::vector<std::string> events;
    void titleChanged(std::string value){title.set(value);events.push_back("title:"+value);}
    void notesChanged(std::string value){notes.set(value);events.push_back("notes:"+value);}
    void notesSubmitted(std::string value){events.push_back("submit:"+value);}
    void edited(std::string name,const lvui::ListItemValue&){events.push_back("edit:"+name);}
    void actionValues(std::string name,const lvui::ListItemActionValues& values){
        events.push_back("values:"+name+":"+values.input_text1+":"+values.input_text2+":"+std::to_string(values.checked));
    }
    void actionEvent(std::string name,const lvui::ListItemActionPayload& payload){
        if(const auto* submission=std::get_if<lvui::ListItemInputSubmission>(&payload))
            events.push_back("event:"+name+":"+submission->field+":"+submission->text);
        else if(const auto* values=std::get_if<lvui::ListItemActionValues>(&payload))
            events.push_back("event:"+name+":"+values->input_text1+":"+values->input_text2);
    }
};
