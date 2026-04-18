/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifdef ENABLE_SCRIPTING

    #include "../../core/EnumMap.hpp"
    #include "../../ride/ShopItem.h"
    #include "../ScriptUtil.hpp"

namespace OpenRCT2::Scripting
{
    static const EnumMap<ShopItem> ShopItemMap(
        {
            { "beef_noodles", ShopItem::beefNoodles },
            { "burger", ShopItem::burger },
            { "candyfloss", ShopItem::candyfloss },
            { "chicken", ShopItem::chicken },
            { "chips", ShopItem::chips },
            { "chocolate", ShopItem::chocolate },
            { "cookie", ShopItem::cookie },
            { "doughnut", ShopItem::doughnut },
            { "hot_dog", ShopItem::hotDog },
            { "fried_rice_noodles", ShopItem::friedRiceNoodles },
            { "funnel_cake", ShopItem::funnelCake },
            { "ice_cream", ShopItem::iceCream },
            { "meatball_soup", ShopItem::meatballSoup },
            { "pizza", ShopItem::pizza },
            { "popcorn", ShopItem::popcorn },
            { "pretzel", ShopItem::pretzel },
            { "roast_sausage", ShopItem::roastSausage },
            { "sub_sandwich", ShopItem::subSandwich },
            { "tentacle", ShopItem::tentacle },
            { "toffee_apple", ShopItem::toffeeApple },
            { "wonton_soup", ShopItem::wontonSoup },
            { "coffee", ShopItem::coffee },
            { "drink", ShopItem::drink },
            { "fruit_juice", ShopItem::fruitJuice },
            { "iced_tea", ShopItem::icedTea },
            { "lemonade", ShopItem::lemonade },
            { "soybean_milk", ShopItem::soybeanMilk },
            { "sujeonggwa", ShopItem::sujeonggwa },
            { "balloon", ShopItem::balloon },
            { "hat", ShopItem::hat },
            { "map", ShopItem::map },
            { "sunglasses", ShopItem::sunglasses },
            { "toy", ShopItem::toy },
            { "tshirt", ShopItem::tShirt },
            { "umbrella", ShopItem::umbrella },
            { "photo1", ShopItem::photo },
            { "photo2", ShopItem::photo2 },
            { "photo3", ShopItem::photo3 },
            { "photo4", ShopItem::photo4 },
            { "voucher", ShopItem::voucher },
            { "empty_bottle", ShopItem::emptyBottle },
            { "empty_bowl_blue", ShopItem::emptyBowlBlue },
            { "empty_bowl_red", ShopItem::emptyBowlRed },
            { "empty_box", ShopItem::emptyBox },
            { "empty_burger_box", ShopItem::emptyBurgerBox },
            { "empty_can", ShopItem::emptyCan },
            { "empty_cup", ShopItem::emptyCup },
            { "empty_drink_carton", ShopItem::emptyDrinkCarton },
            { "empty_juice_cup", ShopItem::emptyJuiceCup },
            { "rubbish", ShopItem::rubbish },
            { "admission", ShopItem::admission },
            { "none", ShopItem::none },
        });

    // Since the ShopItem enum is missing values and includes ShopItem::admission (something a
    // guest cannot carry), 6 is subtracted from the value.
    static_assert((EnumValue(ShopItem::count) - 6) == 50, "ShopItem::count changed, update scripting binding!");
} // namespace OpenRCT2::Scripting

#endif
