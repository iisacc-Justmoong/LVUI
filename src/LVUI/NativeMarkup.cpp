#include "Language.h"
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <map>
#include <cctype>

namespace lvui {
namespace {
std::string_view trim(std::string_view text){const auto first=text.find_first_not_of(" \t\r\n");return first==text.npos?std::string_view{}:text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);}
struct Markup final {
    std::string_view source;std::size_t offset=0,line=1,column=1,nodes=0;
    std::vector<BackendImport> imports;std::vector<Binding> bindings;
    const StyleTokenOptions& style_tokens;
    SourcePosition position()const{return {offset,line,column};}
    [[noreturn]]void fail(std::string message)const{throw Diagnostic{offset,line,column,std::move(message)};}
    char peek()const{return offset<source.size()?source[offset]:'\0';}
    bool starts(std::string_view value)const{return source.substr(offset).starts_with(value);}
    char take(){if(offset>=source.size())fail("Unexpected end of native markup");const auto c=source[offset++];if(c=='\n'){++line;column=1;}else ++column;return c;}
    void skip(){while(std::isspace(static_cast<unsigned char>(peek())))take();}
    void expect(char c){if(take()!=c)fail(std::string("Expected '")+c+"'");}
    void comment(){if(!starts("<!--"))return;for(unsigned i=0;i<4;++i)take();while(!starts("-->")){if(!peek())fail("Unclosed markup comment");take();}take();take();take();}
    std::string name(){std::string result;while(std::isalnum(static_cast<unsigned char>(peek()))||peek()=='_'||peek()=='-'||peek()==':')result+=take();if(result.empty())fail("Expected a markup name");return result;}
    std::string entities(std::string_view text){
        std::string result;
        for(std::size_t i=0;i<text.size();){if(text[i]!='&'){result+=text[i++];continue;}const auto end=text.find(';',i);if(end==text.npos)fail("Unclosed text entity");const auto entity=text.substr(i,end-i+1);
            if(entity=="&amp;")result+='&';else if(entity=="&lt;")result+='<';else if(entity=="&gt;")result+='>';else if(entity=="&quot;")result+='"';else if(entity=="&apos;")result+='\'';else fail("Use a standard XML text entity or literal UTF-8");i=end+1;}
        return result;
    }
    Value expression(std::string_view text){
        auto parsed=parse_view("Window { Label(text: "+std::string(trim(text))+") }");
        if(!parsed)fail("Invalid markup expression: "+parsed.error().message);
        return parsed->root().children.front().inputs.front().value;
    }
    Value value(){
        if(peek()=='\''||peek()=='"'){const auto delimiter=take();std::string text;while(peek()!=delimiter){if(!peek())fail("Unclosed attribute string");text+=take();}take();return {ValueKind::string,entities(text),{}};}
        if(peek()=='{'){take();std::string text;unsigned depth=1;char string=0;bool escaped=false;
            while(depth){const auto c=take();if(string){text+=c;if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c==string)string=0;continue;}
                if(c=='"'){string=c;text+=c;}else if(c=='{'){if(++depth>64)fail("Markup expression nesting limit exceeded");text+=c;}
                else if(c=='}'){if(--depth)text+=c;}else text+=c;}
            return expression(text);
        }
        fail("Attribute values require quotes or a declarative {expression}");
    }
    void input(View& view,std::string key,Value val,SourcePosition at){
        if(std::ranges::any_of(view.inputs,[&](const auto& item){return item.name==key;}))fail("Duplicate markup input: "+key);
        view.inputs.push_back({std::move(key),std::move(val),at});
    }
    void style(View& view,std::string_view styles,std::string& direction){
        static const std::map<std::string_view, std::string_view> names{
            {"width", "width"},
            {"height", "height"},
            {"gap", "spacing"},
            {"padding", "padding"},
            {"font-size", "fontSize"},
            {"font-weight", "fontWeight"},
            {"line-height", "lineHeight"},
            {"color", "color"},
            {"background", "backgroundColor"},
            {"background-color", "backgroundColor"},
            {"border-color", "borderColor"},
            {"border-width", "borderWidth"},
            {"font-family", "fontFamily"},
            {"border-radius", "cornerRadius"},
            {"flex", "flex"}};
        while(!styles.empty()){auto end=styles.find(';');auto part=trim(styles.substr(0,end));if(end==styles.npos)styles={};else styles.remove_prefix(end+1);if(part.empty())continue;
            const auto colon=part.find(':');if(colon==part.npos)fail("Inline style requires name: value");auto key=trim(part.substr(0,colon)),raw=trim(part.substr(colon+1));
            if(key=="display"){if(raw!="flex")fail("Native markup supports display: flex");continue;}
            if(key=="flex-direction"){if(raw!="row"&&raw!="column")fail("Expected row or column flex direction");direction=raw;continue;}
            if(key=="align-items"){const auto alignment=raw=="flex-start"?".leading":raw=="flex-end"?".trailing":raw=="center"?".center":raw=="stretch"?".stretch":"";if(!*alignment)fail("Unsupported native alignment");input(view,"alignment",expression(alignment),view.position);continue;}
            auto known=names.find(key);if(known==names.end())fail("Unsupported native inline style: "+std::string(key));
            if((key=="width"||key=="height")&&raw=="auto")continue;
            const auto context =
                key == "color" || key == "background" || key == "background-color" || key == "border-color"
                    ? StyleTokenType::color
                : key == "font-family" ? StyleTokenType::string
                                       : StyleTokenType::number;
            auto token = default_style_tokens().style_value(raw, context, style_tokens);
            if (!token)
                fail(token.error().message);
            if (*token)
                input(view, std::string(known->second), style_token_literal(**token), view.position);
            else if (key == "color" || key == "background" || key == "background-color" ||
                     key == "border-color" || key == "font-family")
                input(view, std::string(known->second), {ValueKind::string, std::string(raw), {}},
                      view.position);
            else {if(raw.ends_with("px"))raw.remove_suffix(2);input(view,std::string(known->second),expression(raw),view.position);}
        }
    }
    void script(){
        for(unsigned i=0;i<8;++i)take(); // <script>
        const auto start=offset;while(!starts("</script>")){if(!peek())fail("Unclosed declarative script block");take();}
        auto parsed=parse_view(std::string(source.substr(start,offset-start))+"\nWindow {}");
        if(!parsed)fail("Invalid declarative script: "+parsed.error().message);
        imports=parsed->imports();bindings=parsed->bindings();for(unsigned i=0;i<9;++i)take();
    }
    View view(unsigned depth){
        if(depth>64||++nodes>4096)fail("Native markup tree limit exceeded");
        View result;result.position=position();expect('<');const auto tag=name();
        const auto component=tag=="div"?"VStack":tag=="span"||tag=="label"?"Label":tag=="button"||tag=="Button"?"PushButton":tag=="input"?"InputField":tag;
        result.component=find_keyword(component);if(!result.component||result.component->kind!=KeywordKind::view)fail("Unknown native component: "+tag);
        std::string direction="column";
        skip();while(peek()!='>'&&!starts("/>")){
            auto at=position();auto key=name();skip();Value val{ValueKind::boolean,"true",{}};if(peek()=='='){take();skip();val=value();}
            if(key=="style"){if(val.kind!=ValueKind::string)fail("Inline style must be a literal string");style(result,val.text,direction);}
            else {if(key.starts_with("on:")){key="on"+key.substr(3);if(key.size()>2)key[2]=char(std::toupper(static_cast<unsigned char>(key[2])));}
                input(result,std::move(key),std::move(val),at);}skip();
        }
        if(tag=="div")result.component=find_keyword(direction=="row"?"HStack":"VStack");
        if(starts("/>")){take();take();return result;}expect('>');
        std::string text;
        while(!starts("</")){
            if(!peek())fail("Unclosed native component: "+tag);
            if(starts("<!--")){comment();continue;}
            if(peek()=='<'){if(!trim(text).empty())fail("Use span/Label for mixed native text and children");text.clear();result.children.push_back(view(depth+1));}
            else text+=take();
        }
        take();take();if(name()!=tag)fail("Mismatched native closing tag");skip();expect('>');
        const auto content=trim(text);if(!content.empty()){
            if(!result.children.empty())fail("Use span/Label for mixed native text and children");
            if(result.component->spelling!="Label"&&result.component->spelling!="PushButton"&&result.component->spelling!="LabelButton")fail("Text requires a native label or button");
            const auto val=content.front()=='{'&&content.back()=='}'?expression(content.substr(1,content.size()-2)):Value{ValueKind::string,entities(content),{}};
            input(result,"text",val,result.position);
        }return result;
    }
};
}
ViewParseResult parse_native_markup(std::string_view source, const StyleTokenOptions& style_tokens) {
    if(source.size()>1024*1024||!ii::core::valid_utf8(source)||source.find('\0')!=source.npos)return std::unexpected(Diagnostic{0,1,1,"Native markup must be UTF-8 within 1 MiB"});
    const auto valid = default_style_tokens().validate(style_tokens);
    if (!valid)
        return std::unexpected(Diagnostic{0, 1, 1, valid.error().message});
    try {
        Markup parser{.source = source, .style_tokens = style_tokens};
        parser.skip();
        while (parser.starts("<!--")) {
            parser.comment();
            parser.skip();
        }
        if(parser.starts("<script>")){parser.script();parser.skip();}
        auto root=parser.view(0);parser.skip();while(parser.starts("<!--")){parser.comment();parser.skip();}
        if(parser.peek())parser.fail("Native markup requires one root window");
        if(root.component->spelling!="Window"&&root.component->spelling!="ApplicationWindow")parser.fail("The root native component must be a Window or ApplicationWindow");
        return ViewDocument(std::move(root),std::move(parser.imports),std::move(parser.bindings));
    } catch (Diagnostic& error) {
        return std::unexpected(std::move(error));
    }
}
ViewParseResult parse_native_markup(std::string_view source) { return parse_native_markup(source, {}); }
}
