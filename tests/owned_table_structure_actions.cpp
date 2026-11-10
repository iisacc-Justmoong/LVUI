#include <LVUI/TableStructure.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class DomainFactory final : public lvui::TableStructureFactory {
public:
    lvui::TableResult<lvui::TableRow> make_row(const lvui::TableSnapshot& snapshot, std::size_t index) const override {
        lvui::TableRow result{.key="domain-row", .cells=std::vector<lvui::TableCell>(snapshot.columns.size(), {.text="row-default"})};
        if (index != 1) return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid, "Unexpected row position"});
        return result;
    }
    lvui::TableResult<lvui::TableColumnInsertion> make_column(const lvui::TableSnapshot& snapshot, std::size_t index) const override {
        if (index != 1) return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid, "Unexpected column position"});
        return lvui::TableColumnInsertion{.column={.key="domain-column", .title="Custom"},
            .cells=std::vector<lvui::TableCell>(snapshot.rows.size(), {.text="column-default"})};
    }
};
class InvalidFactory final : public lvui::TableStructureFactory {
public:
    lvui::TableResult<lvui::TableRow> make_row(const lvui::TableSnapshot&, std::size_t) const override {
        return lvui::TableRow{.key="r0", .cells={{}}};
    }
    lvui::TableResult<lvui::TableColumnInsertion> make_column(const lvui::TableSnapshot&, std::size_t) const override {
        return lvui::TableColumnInsertion{.column={.key="bad"}, .cells={}};
    }
};
class FirstFactory final : public lvui::TableStructureFactory {
public:
    mutable int row_calls=0, column_calls=0;
    lvui::TableResult<lvui::TableRow> make_row(const lvui::TableSnapshot& snapshot, std::size_t index) const override {
        ++row_calls;
        if(index!=0)return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid,"Unexpected initial row index"});
        return lvui::TableRow{.key="first-domain-row",.cells=std::vector<lvui::TableCell>(snapshot.columns.size(),{.text="row-default"})};
    }
    lvui::TableResult<lvui::TableColumnInsertion> make_column(const lvui::TableSnapshot& snapshot, std::size_t index) const override {
        ++column_calls;
        if(index!=0)return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid,"Unexpected initial column index"});
        return lvui::TableColumnInsertion{.column={.key="first-domain-column"},
            .cells=std::vector<lvui::TableCell>(snapshot.rows.size(),{.text="column-default"})};
    }
};
}
int main() {
    using namespace lvui;
    auto memory = std::make_shared<TableMemory>(
        std::vector<TableColumn>{{.key="a"}, {.key="b"}, {.key="lvui-column-1"}},
        std::vector<TableRow>{{.key="r0", .cells={{}, {}, {}}}, {.key="lvui-row-1", .cells={{}, {}, {}}}});
    auto snapshot = take(memory->snapshot()); TableDefaultStructureFactory defaults;
    auto row = take(plan_table_structure_action(snapshot, {TableStructureAction::insert_row_after, "r0", 0}, defaults));
    auto row_preview = take(prepare_table_structure(snapshot, row));
    require(row_preview.rows.size() == 3 && row_preview.rows[1].key == "lvui-row-2" &&
            row_preview.rows[1].cells.size() == 3 && row_preview.rows[2].key == "lvui-row-1",
            "Default row insertion lost data or generated a duplicate key");
    auto column = take(plan_table_structure_action(snapshot, {TableStructureAction::insert_column_after, "a", 0}, defaults));
    auto column_preview = take(prepare_table_structure(snapshot, column));
    require(column_preview.columns.size() == 4 && column_preview.columns[1].key == "lvui-column-2" &&
            column_preview.rows.size() == 2 && column_preview.rows[0].cells.size() == 4,
            "Default column insertion lost data or generated a duplicate key");
    DomainFactory domain;
    auto domain_row = take(prepare_table_structure(snapshot, take(plan_table_structure_action(snapshot,
        {TableStructureAction::insert_row_after, "r0", 0}, domain))));
    auto domain_column = take(prepare_table_structure(snapshot, take(plan_table_structure_action(snapshot,
        {TableStructureAction::insert_column_after, "a", 0}, domain))));
    require(domain_row.rows[1].key == "domain-row" && domain_row.rows[1].cells[2].text == "row-default" &&
            domain_column.columns[1].title == "Custom" && domain_column.rows[1].cells[1].text == "column-default",
            "The replaceable factory did not control domain defaults");
    auto delete_row = take(prepare_table_structure(snapshot, take(plan_table_structure_action(snapshot,
        {TableStructureAction::delete_row, "r0", 0}, defaults))));
    auto delete_column = take(prepare_table_structure(snapshot, take(plan_table_structure_action(snapshot,
        {TableStructureAction::delete_column, "a", 0}, defaults))));
    require(delete_row.rows.size() == 1 && delete_row.rows[0].key == "lvui-row-1" &&
            delete_column.columns.size() == 2 && delete_column.rows[0].cells.size() == 2,
            "Deletion targeted the wrong key or failed to preserve alignment");
    InvalidFactory invalid;
    require(!plan_table_structure_action(snapshot, {TableStructureAction::insert_row_after, "r0", 0}, invalid) &&
            !plan_table_structure_action(snapshot, {TableStructureAction::insert_column_after, "a", 0}, invalid) &&
            !plan_table_structure_action(snapshot, {TableStructureAction::delete_row, "missing", 0}, defaults) &&
            !plan_table_structure_action(snapshot, {TableStructureAction::delete_row, "r0", 1}, defaults),
            "An invalid factory, missing key or stale intent was accepted");
    auto one_column = std::make_shared<TableSnapshot>(TableSnapshot{.columns={{.key="a"}}, .rows={{.key="r0", .cells={{}}}}});
    require(!plan_table_structure_action(one_column, {TableStructureAction::delete_column, "a", 0}, defaults) &&
            plan_table_structure_action(one_column, {TableStructureAction::delete_row, "r0", 0}, defaults).has_value(),
            "Deleting the last column or last row did not follow LVRS semantics");
    TableMemoryStructureEditor editor(memory);
    require(take(editor.apply(row)) == 1 && !editor.apply(column) && take(memory->snapshot())->columns.size() == 3,
            "A stale planned action changed the document");
    auto empty=std::make_shared<TableMemory>(); auto initial=take(empty->snapshot()); FirstFactory first;
    auto first_row=take(plan_table_structure_action(initial,{TableStructureAction::insert_first_row,"",0},first));
    auto first_column=take(plan_table_structure_action(initial,{TableStructureAction::insert_first_column,"",0},first));
    auto row_candidate=take(prepare_table_structure(initial,first_row)),column_candidate=take(prepare_table_structure(initial,first_column));
    require(row_candidate.rows.size()==1&&row_candidate.rows[0].key=="first-domain-row"&&row_candidate.rows[0].cells.empty()&&
            column_candidate.columns.size()==1&&column_candidate.columns[0].key=="first-domain-column"&&column_candidate.rows.empty()&&
            take(empty->snapshot())->revision==0,"Initial planning did not preserve an empty source or domain data");
    const int row_calls=first.row_calls,column_calls=first.column_calls;
    require(!plan_table_structure_action(initial,{TableStructureAction::insert_first_row,"unexpected",0},first)&&
            !plan_table_structure_action(initial,{TableStructureAction::insert_first_column,"unexpected",0},first)&&
            !plan_table_structure_action(initial,{TableStructureAction::insert_first_row,"",1},first)&&
            !plan_table_structure_action(initial,{TableStructureAction::insert_row_after,"",0},first)&&
            first.row_calls==row_calls&&first.column_calls==column_calls,
            "Invalid first actions reached the domain factory");
    TableMemoryStructureEditor empty_editor(empty);require(take(empty_editor.apply(first_row))==1&&!empty_editor.apply(first_column),
        "A stale first action changed the document");
    auto one_row=take(empty->snapshot());
    require(!plan_table_structure_action(one_row,{TableStructureAction::insert_first_row,"",1},first),
        "A first-row action was accepted on a nonempty row set");
    auto next_column=take(plan_table_structure_action(one_row,{TableStructureAction::insert_first_column,"",1},first));
    require(take(empty_editor.apply(next_column))==2,"The first column was not committed");
    auto complete=take(empty->snapshot());
    require(complete->rows[0].cells.size()==1&&complete->rows[0].cells[0].text=="column-default"&&
            !plan_table_structure_action(complete,{TableStructureAction::insert_first_column,"",2},first),
            "Adding the first column lost its cell or accepted another first column");
    auto reverse=std::make_shared<TableMemory>();TableMemoryStructureEditor reverse_editor(reverse);
    require(take(reverse_editor.apply(take(plan_table_structure_action(take(reverse->snapshot()),
        {TableStructureAction::insert_first_column,"",0},first))))==1,"First column failed on a fully empty table");
    require(take(reverse_editor.apply(take(plan_table_structure_action(take(reverse->snapshot()),
        {TableStructureAction::insert_first_row,"",1},first))))==2&&
        take(reverse->snapshot())->rows[0].cells[0].text=="row-default",
        "First row did not use the existing column or factory defaults");
}
