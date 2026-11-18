#pragma once

#include <LVUI/Text.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui {

inline constexpr std::size_t list_maximum_items=65536;
enum class ListErrorCode { invalid, not_found, stale, exhausted, wrong_thread, busy };
struct ListError final { ListErrorCode code; std::string message; };
template<class T>using ListResult=std::expected<T,ListError>;
struct ListEntry final {
    std::string key,label,description,icon_name;
    bool enabled=true,selectable=true,editable=true,draggable=true;
    friend bool operator==(const ListEntry&,const ListEntry&)=default;
};
struct ListSnapshot final { std::vector<ListEntry> entries; std::uint64_t revision=0; };
class ListSource {
public:
    virtual ~ListSource()=default;
    [[nodiscard]] virtual ListResult<std::shared_ptr<const ListSnapshot>> snapshot()const=0;
};
enum class ListEditKind { insert, replace, erase, move };
struct ListEdit final {
    std::uint64_t revision=0;
    ListEditKind kind=ListEditKind::insert;
    std::string key,before;
    ListEntry item;
};
class ListEditor {
public:
    virtual ~ListEditor()=default;
    [[nodiscard]] virtual ListResult<std::uint64_t> apply(const ListEdit&)=0;
};

// An immutable indexed read view shares unchanged row records across revisions.
class ListIndex final {
public:
    [[nodiscard]] static ListResult<ListIndex> create(std::shared_ptr<const ListSnapshot> snapshot) {
        if (!snapshot || snapshot->entries.size() > list_maximum_items)
            return error(ListErrorCode::invalid, "Invalid list snapshot size");
        auto initial = initialize(snapshot->entries, snapshot->revision, [&](const ListEntry& entry) {
            return Row(snapshot, &entry);
        });
        if (initial) initial->data_->snapshot = std::move(snapshot);
        return initial;
    }

    [[nodiscard]] std::size_t size() const noexcept { return data_->size; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return data_->revision; }
    [[nodiscard]] const ListEntry& entry(std::size_t index) const { return *row(index); }
    [[nodiscard]] std::shared_ptr<const ListEntry> row(std::size_t index) const {
        return data_->pages.at(index / page_size)->at(index % page_size);
    }
    [[nodiscard]] std::optional<std::size_t> find(std::string_view key) const {
        const auto found = data_->positions->find(key);
        if (found == data_->positions->end()) return std::nullopt;
        return found->second;
    }
    [[nodiscard]] std::shared_ptr<const ListSnapshot> snapshot() const {
        std::scoped_lock lock(data_->snapshot_mutex);
        if (!data_->snapshot) {
            auto snapshot = std::make_shared<ListSnapshot>();
            snapshot->revision = revision();
            snapshot->entries.reserve(size());
            for (std::size_t i = 0; i < size(); ++i) snapshot->entries.push_back(entry(i));
            data_->snapshot = std::move(snapshot);
        }
        return data_->snapshot;
    }

    [[nodiscard]] bool operator==(const ListIndex& other) const noexcept { return data_ == other.data_; }

    [[nodiscard]] ListResult<ListIndex> edited(const ListEdit& edit) const {
        if (edit.revision != revision()) return error(ListErrorCode::stale, "List changed before the edit");
        if (unsigned(edit.kind) > unsigned(ListEditKind::move))
            return error(ListErrorCode::invalid, "Invalid list edit kind");
        const bool inserting = edit.kind == ListEditKind::insert;
        if (inserting && !edit.key.empty())
            return error(ListErrorCode::invalid, "List insertion uses the new item's key");
        const auto current = find(edit.key);
        if (!inserting && !current) return error(ListErrorCode::not_found, "List item was not found");
        if (edit.kind == ListEditKind::replace) {
            if (!edit.before.empty() || edit.item.key != edit.key)
                return error(ListErrorCode::invalid, "List replacement must preserve its key");
            if (entry(*current) == edit.item) return *this;
            return replace(*current, edit.item);
        }

        if (edit.kind == ListEditKind::erase && !edit.before.empty())
            return error(ListErrorCode::invalid, "List removal does not take a destination");
        const auto destination = edit.before.empty() ? std::optional(size()) : find(edit.before);
        if (!destination) return error(ListErrorCode::not_found, "List destination was not found");
        if (edit.kind == ListEditKind::move && (*destination == *current || *destination == *current + 1))
            return *this;
        if (revision() == (std::numeric_limits<std::uint64_t>::max)())
            return error(ListErrorCode::exhausted, "List revision exhausted");

        auto bytes = data_->bytes;
        Row inserted;
        if (inserting) {
            if (size() == list_maximum_items || find(edit.item.key))
                return error(ListErrorCode::invalid, "List insertion exceeds its size or duplicates a key");
            auto length = validate_entry(edit.item, bytes);
            if (!length) return std::unexpected(length.error());
            bytes += *length;
            inserted = std::make_shared<const ListEntry>(edit.item);
        } else if (edit.kind == ListEditKind::erase) {
            bytes -= text_size(entry(*current));
        }

        // Structural edits reorder handles; row values and retained revisions stay immutable.
        std::vector<Row> order;
        order.reserve(size() + std::size_t(inserting));
        for (const auto& page : data_->pages) order.insert(order.end(), page->begin(), page->end());
        if (inserting) {
            order.insert(order.begin() + *destination, std::move(inserted));
        } else if (edit.kind == ListEditKind::erase) {
            order.erase(order.begin() + *current);
        } else if (*destination < *current) {
            std::rotate(order.begin() + *destination, order.begin() + *current, order.begin() + *current + 1);
        } else {
            std::rotate(order.begin() + *current, order.begin() + *current + 1, order.begin() + *destination);
        }
        return publish(std::move(order), revision() + 1, bytes, data_.get());
    }

private:
    friend class ListMemory;
    using Row = std::shared_ptr<const ListEntry>;
    using Page = std::vector<Row>;
    inline static constexpr std::size_t page_size = 64;
    inline static constexpr std::size_t maximum_text = 64 * 1024 * 1024;
    struct KeyHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view key) const noexcept { return std::hash<std::string_view>{}(key); }
    };
    using Positions = std::unordered_map<std::string, std::size_t, KeyHash, std::equal_to<>>;
    struct Data {
        std::vector<std::shared_ptr<const Page>> pages;
        std::shared_ptr<const Positions> positions;
        std::size_t size = 0, bytes = 0;
        std::uint64_t revision = 0;
        mutable std::mutex snapshot_mutex;
        mutable std::shared_ptr<const ListSnapshot> snapshot;
    };

    explicit ListIndex(std::shared_ptr<const Data> data) : data_(std::move(data)) {}

    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code, std::string message) {
        return std::unexpected(ListError{code, std::move(message)});
    }
    [[nodiscard]] static std::size_t text_size(const ListEntry& item) noexcept {
        return item.key.size() + item.label.size() + item.description.size() + item.icon_name.size();
    }
    [[nodiscard]] static ListResult<std::size_t> validate_entry(const ListEntry& item, std::size_t used) {
        const auto length = text_size(item);
        if (length > maximum_text - used)
            return error(ListErrorCode::invalid, "List text exceeds its memory limit");
        if (item.key.empty() || item.key.size() > 1024 || item.icon_name.size() > 256
            || !valid_accessibility_text(item.key) || !valid_accessibility_text(item.label)
            || !valid_accessibility_text(item.description) || !valid_accessibility_text(item.icon_name))
            return error(ListErrorCode::invalid, "Invalid list entry");
        return length;
    }
    template<class Entries, class Share>
    [[nodiscard]] static ListResult<ListIndex> initialize(Entries& entries, std::uint64_t revision, Share share) {
        if (entries.size() > list_maximum_items) return error(ListErrorCode::invalid, "Invalid list snapshot size");
        std::vector<Row> order;
        order.reserve(entries.size());
        std::size_t bytes = 0;
        for (auto& entry : entries) {
            auto length = validate_entry(entry, bytes);
            if (!length) return std::unexpected(length.error());
            bytes += *length;
            order.push_back(share(entry));
        }
        return publish(std::move(order), revision, bytes);
    }
    [[nodiscard]] static ListResult<ListIndex> publish(std::vector<Row> order, std::uint64_t revision,
                                                      std::size_t bytes, const Data* previous = nullptr) {
        auto positions = std::make_shared<Positions>();
        positions->reserve(order.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (!positions->emplace(order[i]->key, i).second)
                return error(ListErrorCode::invalid, "Duplicate list key");
        }
        auto data = std::make_shared<Data>();
        data->positions = std::move(positions);
        data->size = order.size();
        data->bytes = bytes;
        data->revision = revision;
        data->pages.reserve((order.size() + page_size - 1) / page_size);
        for (std::size_t offset = 0; offset < order.size(); offset += page_size) {
            const auto end = order.begin() + (std::min)(offset + page_size, order.size());
            const auto begin = order.begin() + offset;
            const auto page = offset / page_size;
            if (previous && page < previous->pages.size()
                && std::ranges::equal(*previous->pages[page], std::ranges::subrange(begin, end))) {
                data->pages.push_back(previous->pages[page]);
            } else {
                data->pages.push_back(std::make_shared<const Page>(begin, end));
            }
        }
        return ListIndex(std::move(data));
    }
    [[nodiscard]] ListResult<ListIndex> replace(std::size_t index, const ListEntry& value) const {
        if (revision() == (std::numeric_limits<std::uint64_t>::max)())
            return error(ListErrorCode::exhausted, "List revision exhausted");
        const auto retained = data_->bytes - text_size(entry(index));
        auto length = validate_entry(value, retained);
        if (!length) return std::unexpected(length.error());
        auto next = std::make_shared<Data>();
        next->pages = data_->pages;
        next->positions = data_->positions;
        next->size = size();
        next->bytes = retained + *length;
        next->revision = revision() + 1;
        auto page = std::make_shared<Page>(*next->pages[index / page_size]);
        page->at(index % page_size) = std::make_shared<const ListEntry>(value);
        next->pages[index / page_size] = std::move(page);
        return ListIndex(std::move(next));
    }

    std::shared_ptr<const Data> data_;
};

// Producers can publish a validated view on their existing revision event.
class IndexedListSource : public ListSource {
public:
    [[nodiscard]] virtual ListResult<ListIndex> index() const = 0;
    [[nodiscard]] ListResult<std::shared_ptr<const ListSnapshot>> snapshot() const final {
        auto current = index();
        if (!current) return std::unexpected(current.error());
        return current->snapshot();
    }
};

class ListMemory final : public IndexedListSource, public ListEditor {
public:
    [[nodiscard]] static ListResult<std::shared_ptr<ListMemory>> create(std::vector<ListEntry> entries = {}) {
        auto initial = ListIndex::initialize(entries, 0, [](ListEntry& entry) {
            return std::make_shared<const ListEntry>(std::move(entry));
        });
        if (!initial) return std::unexpected(initial.error());
        return std::shared_ptr<ListMemory>(new ListMemory(std::move(*initial)));
    }
    [[nodiscard]] ListResult<ListIndex> index() const override {
        std::scoped_lock lock(mutex_);
        return index_;
    }
    [[nodiscard]] ListResult<std::uint64_t> apply(const ListEdit& edit) override {
        std::scoped_lock lock(mutex_);
        auto next = index_.edited(edit);
        if (!next) return std::unexpected(next.error());
        index_ = std::move(*next);
        return index_.revision();
    }
private:
    explicit ListMemory(ListIndex index) : index_(std::move(index)) {}
    mutable std::mutex mutex_;
    ListIndex index_;
};

// Selection is local to one view and never increments a document revision.
class ListController final {
public:
    [[nodiscard]] static ListResult<ListController> create(std::shared_ptr<const ListSource> source){
        if(!source)return error(ListErrorCode::invalid,"List source is required");
        auto indexed = std::dynamic_pointer_cast<const IndexedListSource>(source);
        auto initial = indexed ? indexed->index() : read_snapshot(*source);
        if (!initial) return std::unexpected(initial.error());
        return ListController(std::move(source), std::move(indexed), std::move(*initial));
    }
    [[nodiscard]] ListResult<ListIndex> index() const {
        if (auto r = thread(); !r) return std::unexpected(r.error());
        return index_;
    }
    [[nodiscard]] ListResult<std::shared_ptr<const ListSnapshot>> snapshot()const{
        if(auto r=thread();!r)return std::unexpected(r.error());return index_.snapshot();
    }
    [[nodiscard]] ListResult<std::string> selection()const{
        if(auto r=thread();!r)return std::unexpected(r.error());return selected_;
    }
    [[nodiscard]] ListResult<std::uint64_t> view_revision()const{
        if(auto r=thread();!r)return std::unexpected(r.error());return revision_;
    }
    [[nodiscard]] ListResult<bool> select(std::string_view key){
        if(auto r=mutable_state();!r)return std::unexpected(r.error());
        if(!key.empty()){
            auto found=index_.find(key);if(!found)return error(ListErrorCode::not_found,"List selection was not found");
            const auto& item=index_.entry(*found);
            if(!item.enabled||!item.selectable)return error(ListErrorCode::invalid,"List item cannot be selected");
        }
        if(selected_==key)return false;
        if(revision_==(std::numeric_limits<std::uint64_t>::max)())return error(ListErrorCode::exhausted,"List view revision exhausted");
        selected_=std::string(key);++revision_;return true;
    }
    [[nodiscard]] ListResult<bool> refresh(){
        if(auto r=mutable_state();!r)return std::unexpected(r.error());
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{busy_};
        auto next = updated_index();
        if (!next) return std::unexpected(next.error());
        if (next->revision() < index_.revision())
            return error(ListErrorCode::invalid, "List source revision moved backwards");
        if (next->revision() == index_.revision()) return false;
        bool clear=false;if(!selected_.empty()){
            auto position=next->find(selected_);
            clear=!position || !next->entry(*position).enabled || !next->entry(*position).selectable;
        }
        if(clear&&revision_==(std::numeric_limits<std::uint64_t>::max)())return error(ListErrorCode::exhausted,"List view revision exhausted");
        index_=std::move(*next);if(clear){selected_.clear();++revision_;}return true;
    }
private:
    ListController(std::shared_ptr<const ListSource> source, std::shared_ptr<const IndexedListSource> indexed, ListIndex index)
        : source_(std::move(source)), indexed_(std::move(indexed)), index_(std::move(index)) {}
    [[nodiscard]] static ListResult<ListIndex> read_snapshot(const ListSource& source) {
        auto snapshot = source.snapshot();
        if (!snapshot) return std::unexpected(snapshot.error());
        return ListIndex::create(std::move(*snapshot));
    }
    [[nodiscard]] ListResult<ListIndex> updated_index() const {
        if (indexed_) return indexed_->index();
        auto snapshot = source_->snapshot();
        if (!snapshot) return std::unexpected(snapshot.error());
        if (!*snapshot || (*snapshot)->revision < index_.revision())
            return error(ListErrorCode::invalid, "List source revision moved backwards");
        if ((*snapshot)->revision == index_.revision()) return index_;
        return ListIndex::create(std::move(*snapshot));
    }
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    [[nodiscard]] ListResult<void> thread()const{
        if(owner_!=std::this_thread::get_id())return error(ListErrorCode::wrong_thread,"List controller requires its owner thread");return {};
    }
    [[nodiscard]] ListResult<void> mutable_state()const{
        if(auto r=thread();!r)return r;if(busy_)return error(ListErrorCode::busy,"List controller is refreshing");return {};
    }
    std::shared_ptr<const ListSource> source_;
    std::shared_ptr<const IndexedListSource> indexed_;
    ListIndex index_;
    std::string selected_;
    std::uint64_t revision_=0;
    const std::thread::id owner_=std::this_thread::get_id();
    bool busy_=false;
};

} // namespace lvui
