// Included as a separate translation unit.
// Contains: rebuildDisplayRows, normaliseLayerOrder, hasCustomGroup*,
//           findCustomGroupById, syncCustomGroup*, syncLayerState*,
//           ensureCustomColumns*, nextCustomGroup/Clip*, addCustomGroup,
//           addCustom*TriggerToGroup, addCustom*Trigger, deleteCustom*,
//           deleteTriggerRow, moveCustom*, moveLayerGroup, moveClipRow,
//           beginCustomDrag, updateCustomDrag, endCustomDrag,
//           fireCustomTrigger
#include "../TriggerMainWindow.h"

namespace trigger
{

void TriggerContentComponent::rebuildDisplayRows()
{
    displayRows_.clear();
    syncLayerStateFromCustomGroups();

    std::map<int, std::vector<int>> byLayer;
    for (int i = 0; i < (int) triggerRows_.size(); ++i)
    {
        const auto& clip = triggerRows_[(size_t) i];
        if (! clip.isCustom)
            byLayer[clip.layer].push_back (i);
    }

    normaliseLayerOrder();

    std::vector<int> orderedLayers;
    orderedLayers.reserve ((size_t) layerOrder_.size());
    for (const auto& [layer, _] : layerOrder_)
        orderedLayers.push_back (layer);
    std::sort (orderedLayers.begin(), orderedLayers.end(), [this] (int a, int b)
    {
        const int ao = layerOrder_.count (a) != 0 ? layerOrder_.at (a) : 0;
        const int bo = layerOrder_.count (b) != 0 ? layerOrder_.at (b) : 0;
        if (ao != bo)
            return ao < bo;
        return a < b;
    });

    for (int layer : orderedLayers)
    {
        if (trigger::model::isCustomLayer (layer))
        {
            const int groupId = trigger::model::groupIdFromLayer (layer);
            const auto* group = findCustomGroupById (groupId);
            if (group == nullptr)
                continue;

            std::vector<int> indices;
            for (int i = 0; i < (int) triggerRows_.size(); ++i)
            {
                const auto& clip = triggerRows_[(size_t) i];
                if (clip.isCustom && clip.customGroupId == groupId)
                    indices.push_back (i);
            }

            std::sort (indices.begin(), indices.end(), [this] (int a, int b)
            {
                const auto& ca = triggerRows_[(size_t) a];
                const auto& cb = triggerRows_[(size_t) b];
                if (ca.orderIndex != cb.orderIndex)
                    return ca.orderIndex < cb.orderIndex;
                return ca.customClipId < cb.customClipId;
            });

            layerExpanded_[layer] = group->expanded;
            layerEnabled_[layer] = group->enabled;
            displayRows_.push_back ({ true, layer, -1 });
            if (group->expanded)
                for (int idx : indices)
                    displayRows_.push_back ({ false, layer, idx });
            continue;
        }

        auto it = byLayer.find (layer);
        if (it == byLayer.end())
            continue;

        auto indices = it->second;
        std::sort (indices.begin(), indices.end(), [this] (int a, int b)
        {
            const auto& ca = triggerRows_[(size_t) a];
            const auto& cb = triggerRows_[(size_t) b];
            if (ca.orderIndex != cb.orderIndex)
                return ca.orderIndex < cb.orderIndex;
            return ca.clip < cb.clip;
        });

        if (layerExpanded_.find (layer) == layerExpanded_.end())
            layerExpanded_[layer] = true;
        bool anyIncluded = false;
        for (int idx : indices)
            anyIncluded = anyIncluded || triggerRows_[(size_t) idx].include;
        layerEnabled_[layer] = anyIncluded;

        displayRows_.push_back ({ true, layer, -1 });
        if (layerExpanded_[layer])
            for (int idx : indices)
                displayRows_.push_back ({ false, layer, idx });
    }

}

void TriggerContentComponent::normaliseLayerOrder()
{
    std::vector<int> layers;
    layers.reserve (customGroups_.size() + triggerRows_.size());

    for (const auto& group : customGroups_)
        layers.push_back (trigger::model::layerFromGroupId (group.id));

    for (const auto& clip : triggerRows_)
    {
        if (! clip.isCustom && std::find (layers.begin(), layers.end(), clip.layer) == layers.end())
            layers.push_back (clip.layer);
    }

    for (auto it = layerOrder_.begin(); it != layerOrder_.end();)
    {
        if (std::find (layers.begin(), layers.end(), it->first) == layers.end())
            it = layerOrder_.erase (it);
        else
            ++it;
    }

    int nextOrder = 0;
    for (const auto& [_, order] : layerOrder_)
        nextOrder = juce::jmax (nextOrder, order + 1);

    std::vector<trigger::model::CustomTriggerGroup> orderedGroups = customGroups_;
    trigger::model::normaliseGroupOrder (orderedGroups);
    for (const auto& group : orderedGroups)
    {
        const int layer = trigger::model::layerFromGroupId (group.id);
        if (layerOrder_.find (layer) == layerOrder_.end())
            layerOrder_[layer] = group.orderIndex;
    }

    for (int layer : layers)
    {
        if (layerOrder_.find (layer) == layerOrder_.end())
            layerOrder_[layer] = nextOrder++;
    }

    std::vector<std::pair<int, int>> ordered;
    ordered.reserve (layerOrder_.size());
    for (const auto& [layer, order] : layerOrder_)
        ordered.emplace_back (layer, order);

    std::sort (ordered.begin(), ordered.end(), [] (const auto& a, const auto& b)
    {
        if (a.second != b.second)
            return a.second < b.second;
        return a.first < b.first;
    });

    for (int i = 0; i < (int) ordered.size(); ++i)
    {
        layerOrder_[ordered[(size_t) i].first] = i;
        if (trigger::model::isCustomLayer (ordered[(size_t) i].first))
        {
            const int groupId = trigger::model::groupIdFromLayer (ordered[(size_t) i].first);
            if (auto* group = findCustomGroupById (groupId))
                group->orderIndex = i;
        }
    }
}

bool TriggerContentComponent::hasCustomGroup() const
{
    return ! customGroups_.empty();
}

bool TriggerContentComponent::hasCustomGroupsAtLimit() const
{
    return (int) customGroups_.size() >= trigger::model::kMaxCustomGroups;
}

trigger::model::CustomTriggerGroup* TriggerContentComponent::findCustomGroupById (int groupId)
{
    for (auto& group : customGroups_)
        if (group.id == groupId)
            return &group;
    return nullptr;
}

const trigger::model::CustomTriggerGroup* TriggerContentComponent::findCustomGroupById (int groupId) const
{
    for (const auto& group : customGroups_)
        if (group.id == groupId)
            return &group;
    return nullptr;
}

void TriggerContentComponent::syncCustomGroupStateFromLayers()
{
    for (auto& group : customGroups_)
    {
        const int layer = trigger::model::layerFromGroupId (group.id);
        if (auto it = layerExpanded_.find (layer); it != layerExpanded_.end())
            group.expanded = it->second;
        if (auto it = layerEnabled_.find (layer); it != layerEnabled_.end())
            group.enabled = it->second;
    }
}

void TriggerContentComponent::syncLayerStateFromCustomGroups()
{
    for (const auto& group : customGroups_)
    {
        const int layer = trigger::model::layerFromGroupId (group.id);
        layerExpanded_[layer] = group.expanded;
        layerEnabled_[layer] = group.enabled;
    }
}

void TriggerContentComponent::ensureCustomColumnsState()
{
    if (hasCustomGroup())
        addCustomColumns();
    else
        removeCustomColumns();

    createCustomBtn_.setEnabled (! hasCustomGroupsAtLimit());
}

int TriggerContentComponent::nextCustomGroupId() const
{
    int nextId = 1;
    for (const auto& group : customGroups_)
        nextId = juce::jmax (nextId, group.id + 1);
    return nextId;
}

int TriggerContentComponent::nextCustomClipId() const
{
    int nextId = 1;
    for (const auto& clip : triggerRows_)
        if (clip.isCustom)
            nextId = juce::jmax (nextId, clip.customClipId + 1);
    return nextId;
}

int TriggerContentComponent::nextCustomClipOrder (int groupId) const
{
    int nextOrder = 0;
    for (const auto& clip : triggerRows_)
        if (clip.isCustom && clip.customGroupId == groupId)
            nextOrder = juce::jmax (nextOrder, clip.orderIndex + 1);
    return nextOrder;
}

int TriggerContentComponent::addCustomGroup()
{
    if (hasCustomGroupsAtLimit())
        return 0;

    auto groupId = nextCustomGroupId();
    auto orderIndex = (int) customGroups_.size();
    customGroups_.push_back ({ groupId, "Custom Group " + juce::String (groupId), true, true, orderIndex });
    trigger::model::normaliseGroupOrder (customGroups_);
    ensureCustomColumnsState();
    rebuildDisplayRows();
    refreshTriggerTableContent();
    return groupId;
}

void TriggerContentComponent::addCustomColTriggerToGroup (int groupId)
{
    if (findCustomGroupById (groupId) == nullptr)
        return;

    ensureCustomColumnsState();

    TriggerClip row;
    row.layer = trigger::model::layerFromGroupId (groupId);
    row.clip = nextCustomClipOrder (groupId) + 1;
    row.include = true;
    row.hasOffset = true;
    row.name = "Custom " + juce::String (row.clip);
    row.layerName = findCustomGroupById (groupId)->name;
    row.countdownTc = "00:00:00:00";
    row.triggerRangeSec = 5.0;
    row.durationTc = "00:00:00:00";
    row.triggerTc = "00:00:00:00";
    row.endActionMode = "off";
    row.isCustom = true;
    row.customGroupId = groupId;
    row.customClipId = nextCustomClipId();
    row.orderIndex = nextCustomClipOrder (groupId);
    row.customType = "col";
    row.customSourceCol = "1";
    triggerRows_.push_back (row);
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::addCustomLcTriggerToGroup (int groupId)
{
    if (findCustomGroupById (groupId) == nullptr)
        return;

    ensureCustomColumnsState();

    TriggerClip row;
    row.layer = trigger::model::layerFromGroupId (groupId);
    row.clip = nextCustomClipOrder (groupId) + 1;
    row.include = true;
    row.hasOffset = true;
    row.name = "Layer/Clip";
    row.layerName = findCustomGroupById (groupId)->name;
    row.countdownTc = "00:00:00:00";
    row.triggerRangeSec = 5.0;
    row.durationTc = "00:00:00:00";
    row.triggerTc = "00:00:00:00";
    row.endActionMode = "off";
    row.isCustom = true;
    row.customGroupId = groupId;
    row.customClipId = nextCustomClipId();
    row.orderIndex = nextCustomClipOrder (groupId);
    row.customType = "lc";
    row.customSourceLayer = "1";
    row.customSourceClip = "1";
    triggerRows_.push_back (row);
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::addCustomColTrigger()
{
    int groupId = 0;
    if (! hasCustomGroupsAtLimit())
        groupId = addCustomGroup();
    if (groupId != 0)
        addCustomColTriggerToGroup (groupId);
}

void TriggerContentComponent::addCustomLcTrigger()
{
    int groupId = 0;
    if (! hasCustomGroupsAtLimit())
        groupId = addCustomGroup();
    if (groupId != 0)
        addCustomLcTriggerToGroup (groupId);
}

void TriggerContentComponent::addCustomGcTriggerToGroup (int groupId)
{
    if (findCustomGroupById (groupId) == nullptr)
        return;

    ensureCustomColumnsState();

    TriggerClip row;
    row.layer = trigger::model::layerFromGroupId (groupId);
    row.clip = nextCustomClipOrder (groupId) + 1;
    row.include = true;
    row.hasOffset = true;
    row.name = "Group/Col";
    row.layerName = findCustomGroupById (groupId)->name;
    row.countdownTc = "00:00:00:00";
    row.triggerRangeSec = 5.0;
    row.durationTc = "00:00:00:00";
    row.triggerTc = "00:00:00:00";
    row.endActionMode = "off";
    row.isCustom = true;
    row.customGroupId = groupId;
    row.customClipId = nextCustomClipId();
    row.orderIndex = nextCustomClipOrder (groupId);
    row.customType = "gc";
    row.customSourceGroup = "1";
    row.customSourceCol = "1";
    triggerRows_.push_back (row);
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::addCustomGcTrigger()
{
    int groupId = 0;
    if (! hasCustomGroupsAtLimit())
        groupId = addCustomGroup();
    if (groupId != 0)
        addCustomGcTriggerToGroup (groupId);
}

void TriggerContentComponent::deleteCustomTrigger (int clipIndex)
{
    if (! juce::isPositiveAndBelow (clipIndex, (int) triggerRows_.size()))
        return;
    if (! triggerRows_[(size_t) clipIndex].isCustom)
        return;
    const auto& c = triggerRows_[(size_t) clipIndex];
    currentTriggerKeys_.erase ({ c.layer, c.clip });
    pendingEndActions_.erase ({ c.layer, c.clip });
    triggerRangeActive_.erase ({ c.layer, c.clip });
    const int groupId = c.customGroupId;
    triggerRows_.erase (triggerRows_.begin() + clipIndex);
    int order = 0;
    for (auto& clip : triggerRows_)
    {
        if (clip.isCustom && clip.customGroupId == groupId)
        {
            clip.orderIndex = order++;
            clip.clip = order;
        }
    }
    rebuildDisplayRows();
    refreshTriggerTableContent();
    bool groupStillExists = false;
    for (const auto& clip : triggerRows_)
        if (clip.isCustom && clip.customGroupId == groupId)
            groupStillExists = true;
    if (! groupStillExists)
        deleteCustomGroup (groupId);
    else
        ensureCustomColumnsState();
    repaintTriggerTable();
}

void TriggerContentComponent::deleteTriggerRow (int clipIndex)
{
    if (! juce::isPositiveAndBelow (clipIndex, (int) triggerRows_.size()))
        return;

    if (triggerRows_[(size_t) clipIndex].isCustom)
    {
        deleteCustomTrigger (clipIndex);
        return;
    }

    const auto c = triggerRows_[(size_t) clipIndex];
    currentTriggerKeys_.erase ({ c.layer, c.clip });
    pendingEndActions_.erase ({ c.layer, c.clip });
    triggerRangeActive_.erase ({ c.layer, c.clip });
    triggerRows_.erase (triggerRows_.begin() + clipIndex);

    int order = 0;
    bool groupStillExists = false;
    for (auto& clip : triggerRows_)
    {
        if (! clip.isCustom && clip.layer == c.layer)
        {
            clip.orderIndex = order++;
            groupStillExists = true;
        }
    }

    if (! groupStillExists)
    {
        layerExpanded_.erase (c.layer);
        layerEnabled_.erase (c.layer);
        layerOrder_.erase (c.layer);
        normaliseLayerOrder();
    }

    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::deleteCustomGroup (int groupId)
{
    for (auto it = triggerRows_.begin(); it != triggerRows_.end();)
        it = (it->isCustom && it->customGroupId == groupId) ? triggerRows_.erase (it) : ++it;
    for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        it = (it->first == trigger::model::layerFromGroupId (groupId)) ? currentTriggerKeys_.erase (it) : ++it;
    for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
        it = (it->first.first == trigger::model::layerFromGroupId (groupId)) ? pendingEndActions_.erase (it) : ++it;
    for (auto it = triggerRangeActive_.begin(); it != triggerRangeActive_.end();)
        it = (it->first.first == trigger::model::layerFromGroupId (groupId)) ? triggerRangeActive_.erase (it) : ++it;

    layerExpanded_.erase (trigger::model::layerFromGroupId (groupId));
    layerEnabled_.erase (trigger::model::layerFromGroupId (groupId));
    customGroups_.erase (std::remove_if (customGroups_.begin(), customGroups_.end(),
                                         [groupId] (const auto& group) { return group.id == groupId; }),
                         customGroups_.end());
    trigger::model::normaliseGroupOrder (customGroups_);
    rebuildDisplayRows();
    refreshTriggerTableContent();
    updateWindowHeight (false);
    ensureCustomColumnsState();
    repaintTriggerTable();
    resized();
    repaint();
}

void TriggerContentComponent::deleteLayerGroup (int layer)
{
    if (trigger::model::isCustomLayer (layer))
    {
        deleteCustomGroup (trigger::model::groupIdFromLayer (layer));
        return;
    }

    for (auto it = triggerRows_.begin(); it != triggerRows_.end();)
        it = (! it->isCustom && it->layer == layer) ? triggerRows_.erase (it) : ++it;
    for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        it = (it->first == layer) ? currentTriggerKeys_.erase (it) : ++it;
    for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
        it = (it->first.first == layer) ? pendingEndActions_.erase (it) : ++it;
    for (auto it = triggerRangeActive_.begin(); it != triggerRangeActive_.end();)
        it = (it->first.first == layer) ? triggerRangeActive_.erase (it) : ++it;

    layerExpanded_.erase (layer);
    layerEnabled_.erase (layer);
    layerOrder_.erase (layer);
    deletedLayers_.insert (layer);
    normaliseLayerOrder();
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::moveCustomGroup (int groupId, int delta)
{
    trigger::model::normaliseGroupOrder (customGroups_);
    auto* group = findCustomGroupById (groupId);
    if (group == nullptr)
        return;

    const int oldIndex = group->orderIndex;
    const int newIndex = juce::jlimit (0, juce::jmax (0, (int) customGroups_.size() - 1), oldIndex + delta);
    if (newIndex == oldIndex)
        return;

    for (auto& other : customGroups_)
    {
        if (other.id == groupId)
            continue;
        if (delta < 0 && other.orderIndex >= newIndex && other.orderIndex < oldIndex)
            ++other.orderIndex;
        else if (delta > 0 && other.orderIndex <= newIndex && other.orderIndex > oldIndex)
            --other.orderIndex;
    }
    group->orderIndex = newIndex;
    trigger::model::normaliseGroupOrder (customGroups_);
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::moveCustomGroupToOrder (int groupId, int newOrder)
{
    trigger::model::normaliseGroupOrder (customGroups_);
    auto* group = findCustomGroupById (groupId);
    if (group == nullptr)
        return;

    const int oldOrder = group->orderIndex;
    const int clamped = juce::jlimit (0, juce::jmax (0, (int) customGroups_.size() - 1), newOrder);
    if (clamped == oldOrder)
        return;

    for (auto& other : customGroups_)
    {
        if (other.id == groupId)
            continue;
        if (clamped < oldOrder && other.orderIndex >= clamped && other.orderIndex < oldOrder)
            ++other.orderIndex;
        else if (clamped > oldOrder && other.orderIndex <= clamped && other.orderIndex > oldOrder)
            --other.orderIndex;
    }
    group->orderIndex = clamped;
    trigger::model::normaliseGroupOrder (customGroups_);
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::moveCustomClip (int clipIndex, int delta)
{
    if (! juce::isPositiveAndBelow (clipIndex, (int) triggerRows_.size()))
        return;
    auto& clip = triggerRows_[(size_t) clipIndex];
    if (! clip.isCustom)
        return;

    std::vector<int> indices;
    for (int i = 0; i < (int) triggerRows_.size(); ++i)
    {
        const auto& candidate = triggerRows_[(size_t) i];
        if (candidate.isCustom && candidate.customGroupId == clip.customGroupId)
            indices.push_back (i);
    }
    std::sort (indices.begin(), indices.end(), [this] (int a, int b)
    {
        const auto& ca = triggerRows_[(size_t) a];
        const auto& cb = triggerRows_[(size_t) b];
        if (ca.orderIndex != cb.orderIndex)
            return ca.orderIndex < cb.orderIndex;
        return ca.customClipId < cb.customClipId;
    });

    int currentIndex = -1;
    for (int i = 0; i < (int) indices.size(); ++i)
        if (indices[(size_t) i] == clipIndex)
            currentIndex = i;
    if (currentIndex < 0)
        return;

    const int targetIndex = juce::jlimit (0, juce::jmax (0, (int) indices.size() - 1), currentIndex + delta);
    if (targetIndex == currentIndex)
        return;
    std::swap (indices[(size_t) currentIndex], indices[(size_t) targetIndex]);
    for (int i = 0; i < (int) indices.size(); ++i)
    {
        auto& row = triggerRows_[(size_t) indices[(size_t) i]];
        row.orderIndex = i;
        row.clip = i + 1;
    }
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::moveLayerGroup (int layer, int delta)
{
    normaliseLayerOrder();

    std::vector<int> orderedLayers;
    orderedLayers.reserve (layerOrder_.size());
    for (const auto& [candidateLayer, _] : layerOrder_)
        orderedLayers.push_back (candidateLayer);

    std::sort (orderedLayers.begin(), orderedLayers.end(), [this] (int a, int b)
    {
        if (layerOrder_[a] != layerOrder_[b])
            return layerOrder_[a] < layerOrder_[b];
        return a < b;
    });

    auto it = std::find (orderedLayers.begin(), orderedLayers.end(), layer);
    if (it == orderedLayers.end())
        return;

    const int currentIndex = (int) std::distance (orderedLayers.begin(), it);
    const int targetIndex = juce::jlimit (0, juce::jmax (0, (int) orderedLayers.size() - 1), currentIndex + delta);
    if (targetIndex == currentIndex)
        return;

    std::swap (orderedLayers[(size_t) currentIndex], orderedLayers[(size_t) targetIndex]);
    for (int i = 0; i < (int) orderedLayers.size(); ++i)
    {
        layerOrder_[orderedLayers[(size_t) i]] = i;
        if (trigger::model::isCustomLayer (orderedLayers[(size_t) i]))
        {
            const int groupId = trigger::model::groupIdFromLayer (orderedLayers[(size_t) i]);
            if (auto* group = findCustomGroupById (groupId))
                group->orderIndex = i;
        }
    }

    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::moveClipRow (int clipIndex, int delta)
{
    if (! juce::isPositiveAndBelow (clipIndex, (int) triggerRows_.size()))
        return;

    auto& clip = triggerRows_[(size_t) clipIndex];
    if (clip.isCustom)
    {
        moveCustomClip (clipIndex, delta);
        return;
    }

    std::vector<int> indices;
    for (int i = 0; i < (int) triggerRows_.size(); ++i)
    {
        const auto& candidate = triggerRows_[(size_t) i];
        if (! candidate.isCustom && candidate.layer == clip.layer)
            indices.push_back (i);
    }

    std::sort (indices.begin(), indices.end(), [this] (int a, int b)
    {
        const auto& ca = triggerRows_[(size_t) a];
        const auto& cb = triggerRows_[(size_t) b];
        if (ca.orderIndex != cb.orderIndex)
            return ca.orderIndex < cb.orderIndex;
        return ca.clip < cb.clip;
    });

    int currentIndex = -1;
    for (int i = 0; i < (int) indices.size(); ++i)
    {
        if (indices[(size_t) i] == clipIndex)
        {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex < 0)
        return;

    const int targetIndex = juce::jlimit (0, juce::jmax (0, (int) indices.size() - 1), currentIndex + delta);
    if (targetIndex == currentIndex)
        return;

    std::swap (indices[(size_t) currentIndex], indices[(size_t) targetIndex]);
    for (int i = 0; i < (int) indices.size(); ++i)
        triggerRows_[(size_t) indices[(size_t) i]].orderIndex = i;

    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::moveCustomClipToOrder (int clipIndex, int newOrder)
{
    if (! juce::isPositiveAndBelow (clipIndex, (int) triggerRows_.size()))
        return;
    auto& clip = triggerRows_[(size_t) clipIndex];
    if (! clip.isCustom)
        return;

    std::vector<int> indices;
    for (int i = 0; i < (int) triggerRows_.size(); ++i)
    {
        const auto& candidate = triggerRows_[(size_t) i];
        if (candidate.isCustom && candidate.customGroupId == clip.customGroupId)
            indices.push_back (i);
    }
    std::sort (indices.begin(), indices.end(), [this] (int a, int b)
    {
        const auto& ca = triggerRows_[(size_t) a];
        const auto& cb = triggerRows_[(size_t) b];
        if (ca.orderIndex != cb.orderIndex)
            return ca.orderIndex < cb.orderIndex;
        return ca.customClipId < cb.customClipId;
    });

    int currentPos = -1;
    for (int i = 0; i < (int) indices.size(); ++i)
        if (indices[(size_t) i] == clipIndex)
            currentPos = i;
    if (currentPos < 0)
        return;

    const int clamped = juce::jlimit (0, juce::jmax (0, (int) indices.size() - 1), newOrder);
    if (clamped == currentPos)
        return;

    const int movedIndex = indices[(size_t) currentPos];
    indices.erase (indices.begin() + currentPos);
    indices.insert (indices.begin() + clamped, movedIndex);

    for (int i = 0; i < (int) indices.size(); ++i)
    {
        auto& row = triggerRows_[(size_t) indices[(size_t) i]];
        row.orderIndex = i;
        row.clip = i + 1;
    }
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::beginCustomDrag (bool draggingGroup, int identifier)
{
    dragActive_ = true;
    dragGroup_ = draggingGroup;
    dragIdentifier_ = identifier;
    dragHoverRow_ = -1;
    repaint();
}

void TriggerContentComponent::updateCustomDrag (juce::Point<int> screenPoint)
{
    if (! dragActive_)
        return;

    const auto local = triggerTable_.getLocalPoint (nullptr, screenPoint);
    const int insertionIndex = triggerTable_.getInsertionIndexForPosition (local.x, local.y);
    int hoverRow = -1;

    if (dragGroup_)
    {
        int customHeaderSeen = 0;
        hoverRow = (int) displayRows_.size() - 1;
        for (int row = 0; row < (int) displayRows_.size(); ++row)
        {
            const auto& dr = displayRows_[(size_t) row];
            if (! dr.isGroup || ! trigger::model::isCustomLayer (dr.layer))
                continue;
            if (customHeaderSeen >= insertionIndex)
            {
                hoverRow = row;
                break;
            }
            ++customHeaderSeen;
            hoverRow = row;
        }
    }
    else
    {
        const auto* dragged = juce::isPositiveAndBelow (dragIdentifier_, (int) triggerRows_.size())
            ? &triggerRows_[(size_t) dragIdentifier_]
            : nullptr;
        if (dragged == nullptr || ! dragged->isCustom)
            return;

        int groupPos = 0;
        for (int row = 0; row < (int) displayRows_.size(); ++row)
        {
            const auto& dr = displayRows_[(size_t) row];
            if (dr.isGroup || ! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
                continue;
            const auto& rowClip = triggerRows_[(size_t) dr.clipIndex];
            if (! rowClip.isCustom || rowClip.customGroupId != dragged->customGroupId)
                continue;
            if (groupPos >= insertionIndex)
            {
                hoverRow = row;
                break;
            }
            ++groupPos;
            hoverRow = row;
        }
    }

    if (dragHoverRow_ != hoverRow)
    {
        dragHoverRow_ = hoverRow;
        repaint();
    }
}

void TriggerContentComponent::endCustomDrag (juce::Point<int> screenPoint)
{
    if (! dragActive_)
        return;

    updateCustomDrag (screenPoint);
    if (dragGroup_)
    {
        const auto local = triggerTable_.getLocalPoint (nullptr, screenPoint);
        const int insertionIndex = triggerTable_.getInsertionIndexForPosition (local.x, local.y);
        int targetOrder = 0;
        for (int row = 0; row < juce::jmin (insertionIndex, (int) displayRows_.size()); ++row)
        {
            const auto& dr = displayRows_[(size_t) row];
            if (dr.isGroup && trigger::model::isCustomLayer (dr.layer))
                ++targetOrder;
        }
        moveCustomGroupToOrder (dragIdentifier_, targetOrder);
    }
    else if (juce::isPositiveAndBelow (dragIdentifier_, (int) triggerRows_.size()))
    {
        const auto& dragged = triggerRows_[(size_t) dragIdentifier_];
        if (dragged.isCustom)
        {
            const auto local = triggerTable_.getLocalPoint (nullptr, screenPoint);
            const int insertionIndex = triggerTable_.getInsertionIndexForPosition (local.x, local.y);
            int targetOrder = 0;
            for (int row = 0; row < juce::jmin (insertionIndex, (int) displayRows_.size()); ++row)
            {
                const auto& dr = displayRows_[(size_t) row];
                if (dr.isGroup || ! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
                    continue;
                const auto& rowClip = triggerRows_[(size_t) dr.clipIndex];
                if (rowClip.isCustom && rowClip.customGroupId == dragged.customGroupId)
                    ++targetOrder;
            }
            moveCustomClipToOrder (dragIdentifier_, targetOrder);
        }
    }

    dragActive_ = false;
    dragGroup_ = false;
    dragIdentifier_ = 0;
    dragHoverRow_ = -1;
    repaint();
}

void TriggerContentComponent::fireCustomTrigger (const TriggerClip& clip)
{
    const auto targets = collectResolumeSendTargets (clip.sendTargetIndex);

    juce::String addr;
    if (clip.customType == "col")
    {
        const int col = clip.customSourceCol.getIntValue();
        if (col > 0)
            addr = "/composition/columns/" + juce::String (col) + "/connect";
    }
    else if (clip.customType == "lc")
    {
        const int lay = clip.customSourceLayer.getIntValue();
        const int clp = clip.customSourceClip.getIntValue();
        if (lay > 0 && clp > 0)
            addr = "/composition/layers/" + juce::String (lay)
                   + "/clips/" + juce::String (clp) + "/connect";
    }
    else if (clip.customType == "gc")
    {
        const int grp = clip.customSourceGroup.getIntValue();
        const int col = clip.customSourceCol.getIntValue();
        if (grp > 0 && col > 0)
            addr = "/composition/groups/" + juce::String (grp)
                   + "/columns/" + juce::String (col) + "/connect";
    }

    if (addr.isEmpty())
        return;

    for (const auto& target : targets)
        sendOscPulse (target, addr);
}

} // namespace trigger
