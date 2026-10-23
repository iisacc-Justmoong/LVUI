#include <LVUI/Document.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_set>

namespace lvui {
namespace {
bool space(char c) { return c==' ' || c=='\t' || c=='\n' || c=='\r'; }
bool alpha(char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_'; }
bool name_char(char c) { return alpha(c) || (c>='0' && c<='9') || c=='-'; }
std::string_view trim(std::string_view s) {
    while (!s.empty() && space(s.front())) s.remove_prefix(1);
    while (!s.empty() && space(s.back())) s.remove_suffix(1);
    return s;
}
void scalar(std::string& out, unsigned value) {
    if (value<0x80) out += char(value);
    else if (value<0x800) { out+=char(0xc0|(value>>6)); out+=char(0x80|(value&63)); }
    else if (value<0x10000) {
        out+=char(0xe0|(value>>12)); out+=char(0x80|((value>>6)&63)); out+=char(0x80|(value&63));
    } else {
        out+=char(0xf0|(value>>18)); out+=char(0x80|((value>>12)&63));
        out+=char(0x80|((value>>6)&63)); out+=char(0x80|(value&63));
    }
}
struct Parser final {
    std::string_view source;
    std::size_t position=0, nodes=0;
    std::unordered_set<std::string> ids;

    [[noreturn]] void error(std::size_t offset, std::string message) const {
        Diagnostic diagnostic{offset,1,1,std::move(message)};
        for (std::size_t i=0;i<offset && i<source.size();++i) {
            if (source[i]=='\n') { ++diagnostic.line; diagnostic.column=1; }
            else if ((static_cast<unsigned char>(source[i])&0xc0)!=0x80) ++diagnostic.column;
        }
        throw diagnostic;
    }
    bool starts(std::string_view s) const { return source.substr(position).starts_with(s); }
    void whitespace() { while (position<source.size() && space(source[position])) ++position; }
    void expect(std::string_view s) {
        if (!starts(s)) error(position,"Expected '"+std::string(s)+"'");
        position+=s.size();
    }
    void comment() {
        const auto start=position;
        const auto end=source.find("-->",position+4);
        if (end==source.npos) error(start,"Unclosed comment");
        position=end+3;
    }
    void trivia() {
        for (;;) { whitespace(); if (!starts("<!--")) return; comment(); }
    }
    std::string name() {
        const auto start=position;
        while (position<source.size() && name_char(source[position])) ++position;
        if (position==start || !alpha(source[start])) error(start,"Expected a tag or attribute name");
        return std::string(source.substr(start,position-start));
    }
    std::string decode(std::string_view raw, std::size_t offset) const {
        std::string result;
        for (std::size_t i=0;i<raw.size();) {
            if (raw[i]!='&') { result+=raw[i++]; continue; }
            const auto end=raw.find(';',i+1);
            if (end==raw.npos || end-i>16) error(offset+i,"Invalid or unclosed entity");
            auto token=raw.substr(i+1,end-i-1);
            if (token=="amp") result+='&'; else if (token=="lt") result+='<';
            else if (token=="gt") result+='>'; else if (token=="quot") result+='"';
            else if (token=="apos") result+='\''; else if (token=="nbsp") result+="\xc2\xa0";
            else if (token.starts_with('#')) {
                token.remove_prefix(1); int base=10;
                if (token.starts_with('x')) { token.remove_prefix(1); base=16; }
                unsigned value=0;
                const auto read=std::from_chars(token.data(),token.data()+token.size(),value,base);
                if (token.empty() || read.ec!=std::errc{} || read.ptr!=token.data()+token.size() ||
                    value==0 || value>0x10ffff || (value>=0xd800 && value<=0xdfff))
                    error(offset+i,"Invalid Unicode entity");
                scalar(result,value);
            } else error(offset+i,"Unknown entity");
            i=end+1;
        }
        return result;
    }
    Length length(std::string_view value, std::size_t offset, bool automatic, bool percent) const {
        if (value=="auto" && automatic) return {};
        Unit unit=Unit::pixels;
        if (value.ends_with('%') && percent) { unit=Unit::percent; value.remove_suffix(1); }
        else if (value.ends_with("px")) value.remove_suffix(2);
        const auto number=ii::core::parse_decimal(value);
        if (!number || *number<0 || *number>(unit==Unit::percent?100:8192))
            error(offset,"Invalid length; use nonnegative logical pixels, px, % or auto where supported");
        return {unit,float(*number)};
    }
    Color color(std::string_view value, std::size_t offset) const {
        if (value=="transparent") return {0,0,0,0};
        if (value=="black") return {0,0,0,1};
        if (value=="white") return {1,1,1,1};
        if (!value.starts_with('#')) error(offset,"Invalid color; use hexadecimal, black, white or transparent");
        value.remove_prefix(1);
        const bool small=value.size()==3 || value.size()==4;
        if (!small && value.size()!=6 && value.size()!=8) error(offset,"Invalid hexadecimal color");
        float channels[]{0,0,0,1};
        for (std::size_t i=0;i<value.size()/(small?1:2);++i) {
            auto part=value.substr(i*(small?1:2),small?1:2); unsigned n=0;
            auto read=std::from_chars(part.data(),part.data()+part.size(),n,16);
            if (read.ec!=std::errc{} || read.ptr!=part.data()+part.size()) error(offset,"Invalid hexadecimal color");
            channels[i]=float(n)/(small?15:255);
        }
        return {channels[0],channels[1],channels[2],channels[3]};
    }
    void styles(Style& style, std::string_view input, std::size_t offset) const {
        std::size_t cursor=0;
        while (cursor<input.size()) {
            const auto end=input.find(';',cursor);
            const auto raw=input.substr(cursor,(end==input.npos?input.size():end)-cursor);
            auto declaration=trim(raw);
            const auto at=offset+cursor+(declaration.data()-raw.data());
            cursor=end==input.npos?input.size():end+1;
            if (declaration.empty()) continue;
            const auto colon=declaration.find(':');
            if (colon==declaration.npos) error(at,"Expected ':' in style declaration");
            const auto key=trim(declaration.substr(0,colon)), value=trim(declaration.substr(colon+1));
            if (key=="width") style.width=length(value,at,true,true);
            else if (key=="height") style.height=length(value,at,true,true);
            else if (key=="gap") style.gap=length(value,at,false,false).value;
            else if (key=="border-radius") style.radius=length(value,at,false,false).value;
            else if (key=="background" || key=="background-color") style.background=color(value,at);
            else if (key=="color") style.color=color(value,at);
            else if (key=="font-size") {
                const auto size=length(value,at,false,false).value;
                if (size<8 || size>256) error(at,"font-size must be 8..256 logical pixels");
                style.font_size=size;
            } else if (key=="font-weight") {
                if (value=="normal" || value=="400") style.font_weight=400;
                else if (value=="bold" || value=="700") style.font_weight=700;
                else error(at,"font-weight must be normal, bold, 400 or 700");
            } else if (key=="text-align") {
                if (value=="left") style.text_align=Alignment::left;
                else if (value=="center") style.text_align=Alignment::center;
                else if (value=="right") style.text_align=Alignment::right;
                else error(at,"text-align must be left, center or right");
            } else if (key=="display") {
                if (value!="flex") error(at,"display supports flex only");
            } else if (key=="flex-direction") {
                if (value=="row") style.direction=Direction::row;
                else if (value=="column") style.direction=Direction::column;
                else error(at,"flex-direction must be row or column");
            } else if (key=="padding") {
                std::vector<float> edges;
                auto remaining=value;
                while (!remaining.empty()) {
                    auto n=remaining.find_first_of(" \t\r\n");
                    edges.push_back(length(remaining.substr(0,n),at,false,false).value);
                    if (edges.size()>4) error(at,"padding requires 1..4 lengths");
                    if (n==remaining.npos) break;
                    remaining=trim(remaining.substr(n));
                }
                if (edges.empty()) error(at,"padding requires 1..4 lengths");
                style.padding={edges[0],edges.size()>1?edges[1]:edges[0],
                    edges.size()>2?edges[2]:edges[0],edges.size()>3?edges[3]:(edges.size()>1?edges[1]:edges[0])};
            } else error(at,"Unsupported style property '"+std::string(key)+"'");
        }
    }
    Node element(std::size_t depth) {
        if (depth>64) error(position,"Maximum element depth is 64");
        if (++nodes>4096) error(position,"Maximum element count is 4096");
        expect("<"); const auto tag_at=position;
        Node node; node.tag=name();
        if (node.tag=="text" || node.tag=="p" || node.tag=="h1" || node.tag=="span") node.kind=Kind::text;
        else if (node.tag!="view" && node.tag!="div" && node.tag!="section" && node.tag!="main")
            error(tag_at,"Unsupported tag '"+node.tag+"'");
        if (node.tag=="h1") { node.style.font_size=28; node.style.font_weight=700; }
        std::unordered_set<std::string> attributes;
        for (;;) {
            const auto before=position; whitespace();
            if (starts("/>")) { position+=2; return node; }
            if (starts(">")) { ++position; break; }
            if (position==source.size()) error(position,"Unclosed opening tag");
            if (before==position) error(position,"Expected whitespace before attribute");
            const auto at=position; auto attribute=name();
            if (!attributes.insert(attribute).second) error(at,"Duplicate attribute");
            if (attribute!="style" && attribute!="id") error(at,"Unsupported attribute '"+attribute+"'");
            whitespace(); expect("="); whitespace();
            if (position==source.size() || (source[position]!='\'' && source[position]!='"'))
                error(position,"Attribute values must be quoted");
            const auto quote=source[position++]; const auto start=position;
            const auto end=source.find(quote,position);
            if (end==source.npos) error(start,"Unclosed attribute value");
            auto value=decode(source.substr(start,end-start),start); position=end+1;
            if (attribute=="style") styles(node.style,value,start);
            else {
                if (value.empty() || !alpha(value.front()) || !std::all_of(value.begin(),value.end(),name_char))
                    error(start,"id requires an ASCII identifier");
                if (!ids.insert(value).second) error(start,"Duplicate id");
                node.id=std::move(value);
            }
        }
        std::string content;
        for (;;) {
            if (position==source.size()) error(position,"Unclosed element '"+node.tag+"'");
            if (starts("</")) {
                position+=2; const auto at=position;
                if (name()!=node.tag) error(at,"Closing tag does not match '"+node.tag+"'");
                whitespace(); expect(">"); break;
            }
            if (starts("<!--")) { comment(); continue; }
            if (starts("<")) {
                if (node.kind==Kind::text) error(position,"Text elements cannot contain child tags");
                node.children.push_back(element(depth+1));
            } else {
                auto end=source.find('<',position); if (end==source.npos) end=source.size();
                auto value=decode(source.substr(position,end-position),position);
                if (node.kind==Kind::container && !trim(value).empty())
                    error(position,"Use a text element for text inside a container");
                content+=value; position=end;
            }
        }
        bool pending=false;
        for (const char c:content) {
            if (space(c)) pending=!node.text.empty();
            else { if (pending) node.text+=' '; node.text+=c; pending=false; }
        }
        return node;
    }
};
}
ParseResult parse(std::string_view source) {
    Parser parser{source,0,0,{}};
    try {
        if (source.size()>1024*1024) parser.error(0,"LVUI input exceeds 1 MiB");
        if (source.find('\0')!=source.npos) parser.error(source.find('\0'),"NUL is not allowed in LVUI");
        if (!ii::core::valid_utf8(source)) parser.error(0,"LVUI requires valid UTF-8");
        parser.trivia();
        if (!parser.starts("<")) parser.error(parser.position,"Expected one root element");
        auto root=parser.element(1); parser.trivia();
        if (parser.position!=source.size()) parser.error(parser.position,"Expected exactly one root element");
        return Document(std::move(root));
    } catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
} // namespace lvui
