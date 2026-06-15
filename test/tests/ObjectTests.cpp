/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/drawing/ImageIndexType.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectTypes.h>

using namespace OpenRCT2;

// Minimal concrete subclass of Object for testing.
// Object has pure virtual Load() and Unload() so we need this subclass.
class MockObject : public Object
{
public:
    int loadCallCount = 0;
    int unloadCallCount = 0;

    void Load() override
    {
        loadCallCount++;
    }

    void Unload() override
    {
        unloadCallCount++;
    }
};

// ---------------------------------------------------------------------------
// Tests for IsIntransientObjectType (Object.h line 346-349 - changed file)
// ---------------------------------------------------------------------------

// The PR changes Object.h; IsIntransientObjectType is defined in Object.h.
// It returns true only for ObjectType::audio.

TEST(ObjectTest, IsIntransientObjectType_AudioIsIntransient)
{
    EXPECT_TRUE(IsIntransientObjectType(ObjectType::audio));
}

TEST(ObjectTest, IsIntransientObjectType_RideIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::ride));
}

TEST(ObjectTest, IsIntransientObjectType_SmallSceneryIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::smallScenery));
}

TEST(ObjectTest, IsIntransientObjectType_LargeSceneryIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::largeScenery));
}

TEST(ObjectTest, IsIntransientObjectType_WallsIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::walls));
}

TEST(ObjectTest, IsIntransientObjectType_MusicIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::music));
}

// scenarioMeta is intransient per ObjectTypeIsIntransient but NOT per IsIntransientObjectType
// (the function in Object.h only checks audio). Verify this distinction.
TEST(ObjectTest, IsIntransientObjectType_ScenarioMetaIsTransientPerObjectH)
{
    // IsIntransientObjectType in Object.h only returns true for audio, not scenarioMeta
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::scenarioMeta));
}

TEST(ObjectTest, IsIntransientObjectType_PeepAnimationsIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::peepAnimations));
}

TEST(ObjectTest, IsIntransientObjectType_WaterIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::water));
}

TEST(ObjectTest, IsIntransientObjectType_PathsIsTransient)
{
    EXPECT_FALSE(IsIntransientObjectType(ObjectType::paths));
}

// ---------------------------------------------------------------------------
// Tests for Object destructor (Object.cpp: added ~Object() { UnloadImages(); })
// ---------------------------------------------------------------------------

// The Object destructor now calls UnloadImages(). We verify it does not crash
// when the object has no images loaded (the common case in headless/no-graphics
// mode where _baseImageId stays kImageIndexUndefined).
TEST(ObjectTest, Destructor_DoesNotCrashWithNoImagesLoaded)
{
    // Should construct and destruct cleanly without crash
    EXPECT_NO_THROW({
        MockObject obj;
        // No Load() called, no images allocated
    });
}

TEST(ObjectTest, Destructor_DoesNotCrashAfterMultipleConstruct)
{
    // Allocate and destroy multiple MockObjects to check for double-free etc.
    EXPECT_NO_THROW({
        for (int i = 0; i < 5; i++)
        {
            MockObject obj;
            (void)obj;
        }
    });
}

// Verify unique_ptr holding MockObject also cleans up correctly.
TEST(ObjectTest, Destructor_WorksWithUniquePtr)
{
    EXPECT_NO_THROW({
        auto obj = std::make_unique<MockObject>();
        obj.reset();
    });
}

// ---------------------------------------------------------------------------
// Tests for default-initialized _stringTable and _imageTable (Object.h change)
// The PR changes the member declarations from no explicit init to `{}` init.
// Both are equivalent for default constructors, but we verify the observable
// state of a freshly-constructed Object.
// ---------------------------------------------------------------------------

TEST(ObjectTest, DefaultInit_ImageTableIsEmpty)
{
    MockObject obj;
    // After default construction, no images should be present
    EXPECT_EQ(obj.GetNumImages(), 0u);
}

TEST(ObjectTest, DefaultInit_BaseImageIdIsUndefined)
{
    MockObject obj;
    // Before LoadImages is called, base image id must be undefined
    EXPECT_EQ(obj.GetBaseImageId(), kImageIndexUndefined);
}

// ---------------------------------------------------------------------------
// Tests for ObjectEntryDescriptor operators (Object.cpp – operator== and !=)
// ---------------------------------------------------------------------------

TEST(ObjectEntryDescriptorTest, EqualityOperator_JsonObjects_SameIdentifier)
{
    ObjectEntryDescriptor a("rct2.scenery.mything");
    ObjectEntryDescriptor b("rct2.scenery.mything");
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a != b);
}

TEST(ObjectEntryDescriptorTest, EqualityOperator_JsonObjects_DifferentIdentifier)
{
    ObjectEntryDescriptor a("rct2.scenery.foo");
    ObjectEntryDescriptor b("rct2.scenery.bar");
    EXPECT_NE(a, b);
    EXPECT_FALSE(a == b);
}

TEST(ObjectEntryDescriptorTest, EqualityOperator_JsonObjects_SameTypeDifferentIdentifier)
{
    ObjectEntryDescriptor a(ObjectType::smallScenery, "foo");
    ObjectEntryDescriptor b(ObjectType::smallScenery, "bar");
    EXPECT_NE(a, b);
}

TEST(ObjectEntryDescriptorTest, EqualityOperator_JsonObjects_DifferentTypeSameIdentifier)
{
    ObjectEntryDescriptor a(ObjectType::smallScenery, "same");
    ObjectEntryDescriptor b(ObjectType::largeScenery, "same");
    EXPECT_NE(a, b);
}

TEST(ObjectEntryDescriptorTest, EqualityOperator_JsonObjects_SameTypeAndIdentifier)
{
    ObjectEntryDescriptor a(ObjectType::smallScenery, "rct2.scenery.mything");
    ObjectEntryDescriptor b(ObjectType::smallScenery, "rct2.scenery.mything");
    EXPECT_EQ(a, b);
}

TEST(ObjectEntryDescriptorTest, EqualityOperator_DatVsJson_NeverEqual)
{
    RCTObjectEntry entry{};
    entry.flags = 0x00;
    entry.SetName("TSTOBJ  ");
    entry.checksum = 0x12345678;
    ObjectEntryDescriptor datDesc(entry);
    ObjectEntryDescriptor jsonDesc("rct2.test.obj");
    EXPECT_NE(datDesc, jsonDesc);
    EXPECT_FALSE(datDesc == jsonDesc);
}

TEST(ObjectEntryDescriptorTest, InequalityOperator_Reflexive)
{
    ObjectEntryDescriptor a("rct2.test.obj");
    // An object must equal itself
    EXPECT_EQ(a, a);
    EXPECT_FALSE(a != a);
}

TEST(ObjectEntryDescriptorTest, InequalityOperator_DatObjects_SameEntry)
{
    RCTObjectEntry entry{};
    entry.flags = 0x00;
    entry.SetName("MYTHING ");
    entry.checksum = 0xDEADBEEF;
    ObjectEntryDescriptor a(entry);
    ObjectEntryDescriptor b(entry);
    EXPECT_EQ(a, b);
}

TEST(ObjectEntryDescriptorTest, InequalityOperator_DatObjects_DifferentChecksum_NoSourceFlag)
{
    // Without official source flags, checksum must match for equality.
    RCTObjectEntry entryA{};
    entryA.flags = 0x00; // no source game flags in upper nibble
    entryA.SetName("MYTHING ");
    entryA.checksum = 0xDEADBEEF;

    RCTObjectEntry entryB{};
    entryB.flags = 0x00;
    entryB.SetName("MYTHING ");
    entryB.checksum = 0x00001234; // different checksum

    ObjectEntryDescriptor a(entryA);
    ObjectEntryDescriptor b(entryB);
    EXPECT_NE(a, b);
}

// Boundary case: empty ObjectEntryDescriptor should equal another empty one.
TEST(ObjectEntryDescriptorTest, DefaultConstructed_AreEqual)
{
    ObjectEntryDescriptor a;
    ObjectEntryDescriptor b;
    EXPECT_EQ(a, b);
}