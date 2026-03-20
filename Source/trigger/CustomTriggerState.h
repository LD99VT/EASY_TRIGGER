#pragma once

#include <algorithm>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

namespace trigger::model
{
constexpr int kMaxCustomGroups = 8;

struct CustomTriggerGroup
{
    int id { 0 };
    juce::String name { "Custom Trigger" };
    bool expanded { true };
    bool enabled { true };
    int orderIndex { 0 };
};

struct TableColumnLayout
{
    int id { 0 };
    int visibleIndex { 0 };
    int width { 0 };
    bool visible { true };
};

inline bool isCustomLayer (int layer) noexcept
{
    return layer < 0;
}

inline int layerFromGroupId (int groupId) noexcept
{
    return -juce::jmax (1, groupId);
}

inline int groupIdFromLayer (int layer) noexcept
{
    return layer < 0 ? -layer : 0;
}

inline juce::Array<juce::var> serialiseTableColumns (const juce::TableHeaderComponent& header)
{
    juce::Array<juce::var> columns;
    const int visibleCount = header.getNumColumns (true);
    for (int visibleIndex = 0; visibleIndex < visibleCount; ++visibleIndex)
    {
        const int id = header.getColumnIdOfIndex (visibleIndex, true);
        if (id == 0)
            continue;

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("visible_index", visibleIndex);
        obj->setProperty ("width", header.getColumnWidth (id));
        obj->setProperty ("visible", header.isColumnVisible (id));
        columns.add (juce::var (obj));
    }
    return columns;
}

inline void applyTableColumns (juce::TableHeaderComponent& header, const juce::Array<juce::var>& columns)
{
    struct PendingColumn
    {
        int id { 0 };
        int visibleIndex { 0 };
        int width { 0 };
        bool visible { true };
    };

    std::vector<PendingColumn> pending;
    pending.reserve ((size_t) columns.size());
    for (const auto& entry : columns)
    {
        auto* obj = entry.getDynamicObject();
        if (obj == nullptr)
            continue;
        const int id = (int) obj->getProperty ("id");
        if (header.getIndexOfColumnId (id, false) < 0)
            continue;
        pending.push_back ({
            id,
            (int) obj->getProperty ("visible_index"),
            (int) obj->getProperty ("width"),
            obj->hasProperty ("visible") ? (bool) obj->getProperty ("visible") : true
        });
    }

    std::sort (pending.begin(), pending.end(), [] (const auto& a, const auto& b)
    {
        if (a.visibleIndex != b.visibleIndex)
            return a.visibleIndex < b.visibleIndex;
        return a.id < b.id;
    });

    for (const auto& col : pending)
    {
        header.setColumnVisible (col.id, col.visible);
        if (col.width > 0)
            header.setColumnWidth (col.id, col.width);
        if (col.visible)
            header.moveColumn (col.id, juce::jmax (0, col.visibleIndex));
    }
}

inline void normaliseGroupOrder (std::vector<CustomTriggerGroup>& groups)
{
    std::sort (groups.begin(), groups.end(), [] (const auto& a, const auto& b)
    {
        if (a.orderIndex != b.orderIndex)
            return a.orderIndex < b.orderIndex;
        return a.id < b.id;
    });

    for (size_t i = 0; i < groups.size(); ++i)
        groups[i].orderIndex = (int) i;
}
} // namespace trigger::model
