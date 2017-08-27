#include <gtest/gtest.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/BannerObject.h>

//using namespace OpenRCT2;

class ObjectsTest : public testing::Test
{
protected:
};

TEST_F(ObjectsTest, create_empty_banner)
{
    MemoryStream ms(0);
    ASSERT_EQ(ms.CanRead(), true);
    ASSERT_EQ(ms.CanWrite(), true);
    rct_object_entry entry = {};
    Object * obj = new BannerObject(entry);
    ASSERT_STRCASEEQ(obj->GetName(), "");
    const rct_scenery_entry sceneryEntry = {};
    int res = memcmp(obj->GetLegacyData(), &sceneryEntry, sizeof(rct_scenery_entry));
    ASSERT_EQ(res, 0);
    ASSERT_EQ(obj->GetObjectType(), 0);
    delete obj;
}