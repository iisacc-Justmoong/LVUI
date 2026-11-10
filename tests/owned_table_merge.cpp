#include <LVUI/TableMerge.h>
#include <LVUI/TableClipboard.h>
#include <LVUI/TableEditing.h>
#include <LVUI/TableStructure.h>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
std::shared_ptr<lvui::TableMemory> table(){
    using namespace lvui;
    return std::make_shared<TableMemory>(std::vector<TableColumn>{{.key="a"},{.key="b"},{.key="c"}},
        std::vector<TableRow>{{.key="0",.cells={{.text="A"},{.text="B"},{.text="C"}}},
                              {.key="1",.cells={{.text="D"},{.text="E"},{.text="F"}}},
                              {.key="2",.cells={{.text="G"},{.text="H"},{.text="I"}}}});
}
}
int main(){
    using namespace lvui;auto source=table();auto original=take(source->snapshot());
    const TableMerge first{.row=0,.column=0,.row_count=2,.column_count=2};
    TableMergeEdit merge{.revision=0,.action=TableMergeAction::merge,.range=first};
    auto candidate=take(prepare_table_merge(original,merge));
    require(candidate.merges==std::vector<TableMerge>{first}&&original->merges.empty()&&original->revision==0,
            "Merge preparation changed the source or lost its range");
    auto index=take(TableIndex::create(std::make_shared<const TableSnapshot>(candidate)));
    require(index.merge_at(0,0)==first&&index.merge_at(1,1)==first&&!index.merge_at(2,2),
            "Merge lookup did not resolve anchor and covered cells");
    auto invalid=candidate;invalid.merges.push_back({.row=1,.column=1,.row_count=2,.column_count=2});
    require(!TableIndex::create(std::make_shared<const TableSnapshot>(invalid)),"Overlapping merges were accepted");
    for(const auto& bad:std::vector<TableMerge>{{.row=3,.column=0,.row_count=2,.column_count=1},
            {.row=0,.column=2,.row_count=1,.column_count=2},{.row=0,.column=0,.row_count=0,.column_count=2},
            {.row=0,.column=0,.row_count=1,.column_count=1}}){
        invalid=candidate;invalid.merges={bad};
        require(!TableIndex::create(std::make_shared<const TableSnapshot>(invalid)),"An invalid merged range was accepted");
    }
    TableMemoryMergeEditor editor(source);require(take(editor.apply(merge))==1,"Merge did not publish one revision");
    merge.revision=1;require(take(editor.apply(merge))==1,"An unchanged merge published a revision");
    TableController view(source);
    require(take(view.select({"1","b"}))&&take(view.selection())->current==TablePosition{"0","a"},
            "Selecting a covered cell did not target its merge anchor");
    auto selected=take(view.range());require(selected&&selected->first_row==0&&selected->last_row==1&&
            selected->first_column==0&&selected->last_column==1,"A single merged selection did not cover its full range");
    require(take(view.navigate(TableDirection::right))&&take(view.selection())->current==TablePosition{"0","c"},
            "Right navigation did not skip the current merged range");
    require(take(view.select({"0","a"}))&&take(view.navigate(TableDirection::down))&&
            take(view.selection())->current==TablePosition{"2","a"},"Down navigation did not skip the current merged range");
    auto merged_snapshot=take(source->snapshot());TableClipboardData input{{{"value"}}};
    require(!prepare_table_paste(merged_snapshot,{0,0,0,0},input)&&!prepare_table_paste(merged_snapshot,{1,1,1,1},input)&&
            bool(prepare_table_paste(merged_snapshot,{2,2,2,2},input)),"Paste did not protect merged cells");
    auto editing=take(TableEditing::begin(merged_snapshot,source,{"1","b"}));
    require(take(editing.state()).position==TablePosition{"0","a"}&&take(editing.state()).original=="A"&&bool(editing.cancel()),
            "Editing a covered cell did not use the anchor's value");
    require(!editor.apply({.revision=0,.action=TableMergeAction::split,.range={.row=1,.column=1}})&&
            take(source->snapshot())->revision==1,"A stale split changed the document");
    TableMemoryStructureEditor structure(source);
    require(take(structure.apply({.revision=1,.operations={TableResizeRows{.rows={{.key="0",.height=36}}}}}))==2&&
            take(source->snapshot())->merges==std::vector<TableMerge>{first},"Resizing discarded a merge");
    require(take(editor.apply({.revision=2,.action=TableMergeAction::split,.range={.row=1,.column=1}}))==3,
            "Splitting a covered cell failed");
    auto split=take(source->snapshot());
    require(split->merges.empty()&&split->rows[1].cells[1].text=="E"&&original->rows[0].height==24,
            "Splitting lost hidden cell data or changed the old snapshot");
    require(take(editor.apply({.revision=3,.action=TableMergeAction::split,.range={.row=1,.column=1}}))==3,
            "Splitting an unmerged cell published a revision");
    merge.revision=3;require(take(editor.apply(merge))==4,"A split range could not be merged again");
    require(take(structure.apply({.revision=4,.operations={TableInsertRows{.index=1,.rows={{.key="new",.cells={{},{},{}}}}}}}))==5&&
            take(source->snapshot())->merges.empty(),"Inserting a row retained stale merge geometry");
    auto chained=table();TableMemoryMergeEditor chain_editor(chained);
    require(take(chain_editor.apply({.revision=0,.action=TableMergeAction::merge,
                                     .range={.row=0,.column=0,.row_count=1,.column_count=2}}))==1&&
            take(chain_editor.apply({.revision=1,.action=TableMergeAction::merge,
                                     .range={.row=1,.column=1,.row_count=2,.column_count=2}}))==2,
            "Chained merge setup failed");
    TableController chain_view(chained);
    require(take(chain_view.select({"0","a"}))&&take(chain_view.select({"1","a"},true)),"Chained selection failed");
    auto chain_range=take(chain_view.range());
    require(chain_range&&chain_range->first_row==0&&chain_range->last_row==2&&
            chain_range->first_column==0&&chain_range->last_column==2,
            "Selection did not expand transitively across intersecting merges");
    auto remapped=table();TableController remap_view(remapped);TableMemoryMergeEditor remap_editor(remapped);
    require(take(remap_view.select({"1","b"})),"Pre-merge selection failed");
    const auto view_revision=take(remap_view.view_revision());
    require(take(remap_editor.apply({.revision=0,.action=TableMergeAction::merge,.range=first}))==1&&
            take(remap_view.selection())->current==TablePosition{"0","a"}&&
            take(remap_view.view_revision())==view_revision+1,"An existing selection was not remapped to a new merge anchor");
    auto reorder=take(remapped->snapshot());auto rows=reorder->rows;std::swap(rows[0],rows[1]);
    require(take(remapped->replace(reorder->columns,std::move(rows),1))==2&&take(remapped->snapshot())->merges.empty(),
            "Reordering retained stale merge positions");
    auto overlap=table();TableMemoryMergeEditor overlap_editor(overlap);
    const TableMerge replacement{.row=1,.column=1,.row_count=2,.column_count=2};
    require(take(overlap_editor.apply({.revision=0,.action=TableMergeAction::merge,.range=first}))==1&&
            take(overlap_editor.apply({.revision=1,.action=TableMergeAction::merge,.range=replacement}))==2&&
            take(overlap->snapshot())->merges==std::vector<TableMerge>{replacement},
            "A new overlapping merge did not replace the previous range");
    auto concurrent=table();TableMemoryMergeEditor left(concurrent),right(concurrent);std::atomic<int> winners=0;
    std::thread a([&]{if(left.apply({.revision=0,.action=TableMergeAction::merge,.range=first}))++winners;});
    std::thread b([&]{if(right.apply({.revision=0,.action=TableMergeAction::merge,
                                     .range={.row=1,.column=1,.row_count=2,.column_count=2}}))++winners;});
    a.join();b.join();require(winners==1&&take(concurrent->snapshot())->revision==1,
            "Concurrent merges both committed the same revision");
}
