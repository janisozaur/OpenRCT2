/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

/**
 * Tests for the new ObjectManager code added in this PR:
 *
 * 1. UnloadAll(onlyTransient) now iterates the object repository and unloads
 *    items that have a LoadedObject but were never assigned to a slot.
 *
 * 2. UnloadObjectsExcept() also iterates the repository for the same reason:
 *    loaded-but-not-slotted objects previously leaked.
 *
 * Strategy: Use a minimal stub IObjectRepository (MockObjectRepository) so
 * that the ObjectManager can be exercised in isolation without requiring a
 * full OpenRCT2 context. Only the methods called by the new code are
 * implemented; the others throw to prevent accidental use.
 */

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectRepository.h>
#include <openrct2/object/ObjectTypes.h>
#include <stdexcept>
#include <vector>

using namespace OpenRCT2;

// ---------------------------------------------------------------------------
// MockObject – concrete Object subclass used as a stand-in for real objects.
// Tracks how many times Unload() is called so tests can assert it.
// ---------------------------------------------------------------------------
class TrackingObject : public Object
{
public:
    int unloadCallCount = 0;

    void Load() override
    {
    }

    void Unload() override
    {
        unloadCallCount++;
    }
};

// Helper to build an ObjectEntryDescriptor for a given type.
static ObjectEntryDescriptor MakeDescriptor(ObjectType type, std::string_view id)
{
    return ObjectEntryDescriptor(type, id);
}

// ---------------------------------------------------------------------------
// MockObjectRepository – stub for IObjectRepository.
//
// Keeps a list of ObjectRepositoryItem values. Tests add items to the list
// and then call GetNumObjects() / GetObjects() which return that list.
// UnregisterLoadedObject records whether it was called per-item.
// All other pure-virtual methods throw so accidental calls are caught.
// ---------------------------------------------------------------------------
class MockObjectRepository : public IObjectRepository
{
public:
    std::vector<ObjectRepositoryItem> items;
    std::vector<bool> unregisterCalled; // parallel to items

    void AddItem(ObjectType type, std::shared_ptr<Object> loadedObj = nullptr)
    {
        ObjectRepositoryItem item{};
        item.Id = items.size();
        item.Type = type;
        item.Generation = ObjectGeneration::JSON;
        item.Identifier = "mock.object." + std::to_string(items.size());
        item.LoadedObject = loadedObj;
        items.push_back(std::move(item));
        unregisterCalled.push_back(false);
    }

    // --- IObjectRepository interface ---

    [[nodiscard]] size_t GetNumObjects() const override
    {
        return items.size();
    }

    [[nodiscard]] const ObjectRepositoryItem* GetObjects() const override
    {
        return items.data();
    }

    void UnregisterLoadedObject(const ObjectRepositoryItem* ori, Object* /*object*/) override
    {
        // Find by pointer identity (the pointer comes from items.data())
        auto idx = static_cast<size_t>(ori - items.data());
        if (idx < unregisterCalled.size())
        {
            unregisterCalled[idx] = true;
            // Clear the shared_ptr so the item looks unloaded afterwards
            items[idx].LoadedObject.reset();
        }
    }

    // --- Methods that should not be called by the code under test ---

    void LoadOrConstruct(int32_t) override
    {
        throw std::logic_error("Not expected");
    }
    void Construct(int32_t) override
    {
        throw std::logic_error("Not expected");
    }
    [[nodiscard]] const ObjectRepositoryItem* FindObjectLegacy(std::string_view) const override
    {
        return nullptr;
    }
    [[nodiscard]] const ObjectRepositoryItem* FindObject(std::string_view) const override
    {
        return nullptr;
    }
    [[nodiscard]] const ObjectRepositoryItem* FindObject(const RCTObjectEntry*) const override
    {
        return nullptr;
    }
    [[nodiscard]] const ObjectRepositoryItem* FindObject(const ObjectEntryDescriptor&) const override
    {
        return nullptr;
    }
    [[nodiscard]] std::unique_ptr<Object> LoadObject(const ObjectRepositoryItem*) override
    {
        throw std::logic_error("Not expected");
    }
    [[nodiscard]] std::unique_ptr<Object> LoadObject(const ObjectRepositoryItem*, bool) override
    {
        throw std::logic_error("Not expected");
    }
    void RegisterLoadedObject(const ObjectRepositoryItem*, std::unique_ptr<Object>&&) override
    {
        throw std::logic_error("Not expected");
    }
    void AddObject(const RCTObjectEntry*, const void*, size_t) override
    {
        throw std::logic_error("Not expected");
    }
    void AddObjectFromFile(ObjectGeneration, std::string_view, const void*, size_t) override
    {
        throw std::logic_error("Not expected");
    }
    void ExportPackedObject(IStream*) override
    {
        throw std::logic_error("Not expected");
    }
};

// ---------------------------------------------------------------------------
// Test Fixture
// ---------------------------------------------------------------------------
class ObjectManagerUnloadTests : public testing::Test
{
protected:
    MockObjectRepository repo;
    std::unique_ptr<IObjectManager> manager;

    void SetUp() override
    {
        manager = CreateObjectManager(repo);
    }
};

// ---------------------------------------------------------------------------
// Tests for the new UnloadAll repository loop
// (PR change in ObjectManager.cpp – UnloadAll private method)
// ---------------------------------------------------------------------------

// When a repository item has a non-null LoadedObject and UnloadAll(false) is
// called, Unload() must be called on that object and UnregisterLoadedObject
// must be called on the repository item.
TEST_F(ObjectManagerUnloadTests, UnloadAll_UnloadsRepositoryItemWithLoadedObject)
{
    auto obj = std::make_shared<TrackingObject>();
    obj->SetDescriptor(MakeDescriptor(ObjectType::smallScenery, "mock.small.0"));
    repo.AddItem(ObjectType::smallScenery, obj);

    manager->UnloadAll();

    EXPECT_EQ(obj->unloadCallCount, 1) << "Unload() must be called by the repository loop";
    EXPECT_TRUE(repo.unregisterCalled[0]) << "UnregisterLoadedObject must be called for the item";
}

// A repository item with LoadedObject == nullptr must be left untouched.
TEST_F(ObjectManagerUnloadTests, UnloadAll_SkipsRepositoryItemsWithNullLoadedObject)
{
    repo.AddItem(ObjectType::smallScenery, nullptr);

    // Must not crash, and unregister must not be called.
    EXPECT_NO_THROW(manager->UnloadAll());
    EXPECT_FALSE(repo.unregisterCalled[0]);
}

// UnloadAll must process every non-null item in the repository, regardless of
// how many items there are.
TEST_F(ObjectManagerUnloadTests, UnloadAll_UnloadsAllRepositoryItemsWithLoadedObjects)
{
    auto obj0 = std::make_shared<TrackingObject>();
    auto obj1 = std::make_shared<TrackingObject>();
    auto obj2 = std::make_shared<TrackingObject>();

    obj0->SetDescriptor(MakeDescriptor(ObjectType::ride, "mock.ride.0"));
    obj1->SetDescriptor(MakeDescriptor(ObjectType::water, "mock.water.0"));
    obj2->SetDescriptor(MakeDescriptor(ObjectType::music, "mock.music.0"));

    repo.AddItem(ObjectType::ride, obj0);
    repo.AddItem(ObjectType::water, obj1);
    repo.AddItem(ObjectType::music, obj2);

    manager->UnloadAll();

    EXPECT_EQ(obj0->unloadCallCount, 1);
    EXPECT_EQ(obj1->unloadCallCount, 1);
    EXPECT_EQ(obj2->unloadCallCount, 1);

    EXPECT_TRUE(repo.unregisterCalled[0]);
    EXPECT_TRUE(repo.unregisterCalled[1]);
    EXPECT_TRUE(repo.unregisterCalled[2]);
}

// UnloadAll(false) – full unload – must also unload audio (intransient) items.
TEST_F(ObjectManagerUnloadTests, UnloadAll_FullUnload_AlsoUnloadsAudioItems)
{
    auto audioObj = std::make_shared<TrackingObject>();
    audioObj->SetDescriptor(MakeDescriptor(ObjectType::audio, "mock.audio.0"));
    repo.AddItem(ObjectType::audio, audioObj);

    manager->UnloadAll(); // not transient-only

    EXPECT_EQ(audioObj->unloadCallCount, 1) << "Full UnloadAll must include audio items";
    EXPECT_TRUE(repo.unregisterCalled[0]);
}

// ---------------------------------------------------------------------------
// Tests for the new UnloadAllTransient repository loop
// (PR change in ObjectManager.cpp – onlyTransient=true path)
// ---------------------------------------------------------------------------

// Audio (intransient) items must be skipped when only transient objects are
// being unloaded.
TEST_F(ObjectManagerUnloadTests, UnloadAllTransient_SkipsAudioRepositoryItems)
{
    auto audioObj = std::make_shared<TrackingObject>();
    audioObj->SetDescriptor(MakeDescriptor(ObjectType::audio, "mock.audio.0"));
    repo.AddItem(ObjectType::audio, audioObj);

    manager->UnloadAllTransient();

    EXPECT_EQ(audioObj->unloadCallCount, 0) << "Audio items must NOT be unloaded by UnloadAllTransient";
    EXPECT_FALSE(repo.unregisterCalled[0]) << "UnregisterLoadedObject must NOT be called for audio items";
}

// Non-audio repository items with a LoadedObject must be unloaded even by
// the transient-only call path.
TEST_F(ObjectManagerUnloadTests, UnloadAllTransient_UnloadsNonAudioRepositoryItems)
{
    auto sceneryObj = std::make_shared<TrackingObject>();
    sceneryObj->SetDescriptor(MakeDescriptor(ObjectType::smallScenery, "mock.scenery.0"));
    repo.AddItem(ObjectType::smallScenery, sceneryObj);

    manager->UnloadAllTransient();

    EXPECT_EQ(sceneryObj->unloadCallCount, 1);
    EXPECT_TRUE(repo.unregisterCalled[0]);
}

// Mixed repository: some audio, some non-audio.
// UnloadAllTransient must only unload the non-audio items.
TEST_F(ObjectManagerUnloadTests, UnloadAllTransient_MixedTypes_OnlyUnloadsTransientItems)
{
    auto rideObj = std::make_shared<TrackingObject>();
    auto audioObj = std::make_shared<TrackingObject>();
    auto waterObj = std::make_shared<TrackingObject>();

    rideObj->SetDescriptor(MakeDescriptor(ObjectType::ride, "mock.ride.0"));
    audioObj->SetDescriptor(MakeDescriptor(ObjectType::audio, "mock.audio.0"));
    waterObj->SetDescriptor(MakeDescriptor(ObjectType::water, "mock.water.0"));

    repo.AddItem(ObjectType::ride, rideObj);
    repo.AddItem(ObjectType::audio, audioObj);
    repo.AddItem(ObjectType::water, waterObj);

    manager->UnloadAllTransient();

    // Ride and water must be unloaded
    EXPECT_EQ(rideObj->unloadCallCount, 1) << "ride must be unloaded";
    EXPECT_EQ(waterObj->unloadCallCount, 1) << "water must be unloaded";
    EXPECT_TRUE(repo.unregisterCalled[0]);
    EXPECT_TRUE(repo.unregisterCalled[2]);

    // Audio must be preserved
    EXPECT_EQ(audioObj->unloadCallCount, 0) << "audio must NOT be unloaded";
    EXPECT_FALSE(repo.unregisterCalled[1]);
}

// After UnloadAll, subsequent calls must not double-call Unload() on items
// already cleared (LoadedObject becomes nullptr after UnregisterLoadedObject).
TEST_F(ObjectManagerUnloadTests, UnloadAll_CalledTwice_DoesNotDoubleUnload)
{
    auto obj = std::make_shared<TrackingObject>();
    obj->SetDescriptor(MakeDescriptor(ObjectType::smallScenery, "mock.small.0"));
    repo.AddItem(ObjectType::smallScenery, obj);

    manager->UnloadAll(); // First call: LoadedObject is cleared by mock's UnregisterLoadedObject
    manager->UnloadAll(); // Second call: LoadedObject is nullptr, should be skipped

    EXPECT_EQ(obj->unloadCallCount, 1) << "Unload must be called exactly once";
}

// Null-item mixed with non-null items: null items are skipped, non-null are unloaded.
TEST_F(ObjectManagerUnloadTests, UnloadAll_MixedNullAndNonNullItems)
{
    auto obj = std::make_shared<TrackingObject>();
    obj->SetDescriptor(MakeDescriptor(ObjectType::water, "mock.water.0"));

    repo.AddItem(ObjectType::water, nullptr); // index 0 – null
    repo.AddItem(ObjectType::water, obj);     // index 1 – non-null

    manager->UnloadAll();

    EXPECT_FALSE(repo.unregisterCalled[0]) << "Null item must not trigger UnregisterLoadedObject";
    EXPECT_TRUE(repo.unregisterCalled[1]) << "Non-null item must trigger UnregisterLoadedObject";
    EXPECT_EQ(obj->unloadCallCount, 1);
}